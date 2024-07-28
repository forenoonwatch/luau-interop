import sys
import os
from os import path
from codegen import Codegen

keywordToStringAtom = dict()
stringAtomCounter = 0

def add_atom(key):
    global stringAtomCounter
    keywordToStringAtom[key] = stringAtomCounter
    stringAtomCounter += 1

def init_string_atoms_for_list(nameList):
    for key, value in nameList.items():
        if not key in keywordToStringAtom:
            add_atom(key)

        if 'aliases' in value:
            for alias in value['aliases']:
                add_atom(alias)

def init_keyword_to_string_atom():
    for name, data in codegen.typeDataByName.items():
        filteredProperties = [key for key, value in data['properties'].items() if 'skip_lua_codegen' not in value or not value['skip_lua_codegen']]

        for key in filteredProperties:
            add_atom(key)

        init_string_atoms_for_list(data['functions'])
        init_string_atoms_for_list(data['methods'])
        init_string_atoms_for_list(data['events'])

def gen_useratom(outFile):
    pairs = []

    for atomName, value in keywordToStringAtom.items():
        atomVarName = 'LUA_ATOM_' + Codegen.format_constant_name(atomName)

        pairs.append((
            f"if (sv.compare(\"{atomName}\") == 0) " '{\n'
            '\t\t' f"return {atomVarName};" '\n'
            '\t}'
        ))

    outFile.write((
        '\nint16_t ScriptEnvironment::useratom(const char* s, size_t l) {\n'
        '\tstd::string_view sv(s, l);\n\n\t'
    ))

    outFile.write("\n\telse ".join(pairs))

    outFile.write((
        '\n\n\treturn -1;\n'
        '}\n\n'
    ))

def gen_single_function_wrapper_for_type(data, outFile, funcName, funcData, isMethod, isConstructor,
        argCount=None):
    if 'native_free_function' in funcData or 'native_getter' in funcData or isConstructor:
        wrapperFunctionName = Codegen.get_function_wrapper_name(data['name'], funcName)

        if argCount is not None:
            wrapperFunctionName += '_' + str(argCount)

        outFile.write(f"static int {wrapperFunctionName}(lua_State* L) " '{\n')

        args = []
        ptrCheckArgs = []

        if isMethod:
            selfCheckExpr = f"auto* self = lua_get<{data['name']}>(L, 1);"
            outFile.write(f"\t{selfCheckExpr}\n")

            if 'native_free_function' in funcData:
                args.append('*self')

        for i, paramData in enumerate(funcData['parameters']):
            stackIndex = i + 2 if isMethod else i + 1

            if 'default' in paramData:
                optExpr = Codegen.get_optional_expression(paramData['type'], paramData['name'], stackIndex,
                        paramData['default'])
                outFile.write(f"\t{optExpr}\n")
                args.append(paramData['name'] if Codegen.is_core_type(paramData['type']) else '*' + paramData['name'])
            else:
                checkExpr = Codegen.get_check_expression(paramData['type'], paramData['name'], stackIndex)
                outFile.write(f"\t{checkExpr}\n")

                if not Codegen.is_core_type(paramData['type']):
                    ptrCheckArgs.append(paramData['name'])
                    args.append('*' + paramData['name'])
                else:
                    args.append(paramData['name'])

        if ptrCheckArgs:
            outFile.write('\n\tif (!')
            outFile.write(' || !'.join(ptrCheckArgs))
            outFile.write((
                ') [[unlikely]] {\n'
                '\t\treturn 0;\n'
                '\t}\n'
            ))

        getterExpr = ', '.join(args)

        if not isConstructor:
            if 'native_free_function' in funcData:
                getterExpr = funcData['native_free_function'] + '(' + getterExpr + ')'
            else:
                getterExpr = 'self->' + funcData['native_getter'] + '(' + getterExpr + ')'

        returnType = data['name'] if isConstructor else funcData['return_types'][0]
        pushExpr = Codegen.get_push_expression(returnType, getterExpr)

        outFile.write('\n\t')
        outFile.write(pushExpr)
        outFile.write(';\n\treturn 1;\n')
        outFile.write('}\n\n')

def gen_overload_function_wrapper_for_type(data, outFile, funcName, funcData, isMethod, isConstructor):
    if 'native_lua_function' in funcData:
        return

    for overloadData in funcData['overloads']:
        argCount = len(overloadData['parameters'])
        gen_single_function_wrapper_for_type(data, outFile, funcName, overloadData, isMethod, isConstructor,
                argCount)

    wrapperFunctionName = Codegen.get_function_wrapper_name(data['name'], funcName)

    outFile.write((
        f"static int {wrapperFunctionName}(lua_State* L) " '{\n'
        '\tint argc = lua_gettop(L);\n\n'
        '\tswitch (argc) {\n'
    ))

    for overloadData in funcData['overloads']:
        argCount = len(overloadData['parameters'])
        outFile.write((
            f"\t\tcase {argCount}:\n"
            f"\t\t\treturn {wrapperFunctionName}_{argCount}(L);\n"
        ))

    outFile.write((
        '\t\tdefault:\n'
        '\t\t\tluaL_error(L, "Invalid number of arguments: %d", argc);\n'
        '\t}\n\n'
        '\treturn 0;\n'
        '}\n\n'
    ))

def gen_function_wrapper_for_type(data, outFile, funcName, funcData, isMethod, isConstructor):
    if 'overloads' in funcData:
        gen_overload_function_wrapper_for_type(data, outFile, funcName, funcData, isMethod, isConstructor)
    else:
        gen_single_function_wrapper_for_type(data, outFile, funcName, funcData, isMethod, isConstructor)

def gen_function_wrappers_for_type(data, outFile):
    for methodName, methodData in data['methods'].items():
        gen_function_wrapper_for_type(data, outFile, methodName, methodData, True, False)

    for funcName, funcData in data['functions'].items():
        gen_function_wrapper_for_type(data, outFile, funcName, funcData, False, False)

    for funcName, funcData in data['constructors'].items():
        gen_function_wrapper_for_type(data, outFile, funcName, funcData, False, True)

    if 'metamethods' in data:
        for funcName, funcData in data['metamethods'].items():
            gen_function_wrapper_for_type(data, outFile, funcName, funcData, True, False)

def gen_index_for_type(data, outFile):
    if not data['properties']:
        return

    functionName = Codegen.format_method_name(data['name']) + '_lua_index'
    checkExpr = Codegen.get_check_expression(data['name'], 'obj', 1)

    outFile.write((
        f"int {functionName}(lua_State* L) " '{\n\t'
    ))

    outFile.write(checkExpr)

    outFile.write((
        '\n\tint atom;\n'
        '\tconst char* k = lua_tostringatom(L, 2, &atom);\n\n'
        '\tif (!obj || !k) [[unlikely]] {\n'
        '\t\tluaL_error(L, "Invalid number of arguments %d\\n", lua_gettop(L));\n'
        '\t\treturn 0;\n'
        '\t}\n\n'
        '\tswitch (atom) {\n'
    ))

    cases = []

    for propName, propData in data['properties'].items():
        if 'skip_lua_codegen' in propData and propData['skip_lua_codegen']:
            continue

        atomVarName = 'LUA_ATOM_' + Codegen.format_constant_name(propName)

        if 'native_lua_function' in propData:
            cases.append((
                f"\t\tcase {atomVarName}:\n"
                f"\t\t\treturn {propData['native_lua_function']}(L);\n"
            ))
        else:
            getterExpr = Codegen.get_getter_expression(data, propName, propData)
            pushExpr = Codegen.get_push_expression(propData['type'], getterExpr)

            cases.append((
                f"\t\tcase {atomVarName}:\n"
                f"\t\t\t{pushExpr};\n"
                '\t\t\treturn 1;\n'
            ))

    outFile.write(''.join(cases))

    outFile.write((
        '\t\tdefault:\n'
        '\t\t\tbreak;\n'
        '\t}\n\n'
        f"\tluaL_error(L, \"%s is not a valid member of {data['name']}\", k);\n"
        '\treturn 0;\n'
        '}\n\n'
    ))

def gen_namecall_for_type(data, outFile):
    if not data['methods']:
        return

    functionName = Codegen.format_method_name(data['name']) + '_lua_namecall'

    outFile.write((
        f"int {functionName}(lua_State* L) " '{\n'
        '\tint atom;\n'
        '\tconst char* k = lua_namecallatom(L, &atom);\n\n'
        '\tif (!k) [[unlikely]] {\n'
        '\t\tluaL_error(L, "Invalid namecall!");\n'
        '\t\treturn 0;\n'
        '\t}\n\n'
        '\tswitch (atom) {\n'
    ))

    cases = []

    for methodName, methodData in data['methods'].items():
        if 'skip_lua_codegen' in methodData and methodData['skip_lua_codegen']:
            continue

        atomVarName = 'LUA_ATOM_' + Codegen.format_constant_name(methodName)
        wrapperFunctionName = Codegen.get_function_wrapper_name(data['name'], methodName)
        wrapperFunctionName = methodData['native_lua_function'] if 'native_lua_function' in methodData else wrapperFunctionName

        mainCase = f"\t\tcase {atomVarName}:\n"

        if 'aliases' in methodData:
            mainCase += ''.join([f"\t\tcase LUA_ATOM_{Codegen.format_constant_name(alias)}:\n"
                    for alias in methodData['aliases']])

        cases.append(mainCase + f"\t\t\treturn {wrapperFunctionName}(L);\n")

    outFile.write(''.join(cases))

    outFile.write((
        '\t\tdefault:\n'
        '\t\t\tbreak;\n'
        '\t}\n\n'
        f"\tluaL_error(L, \"%s is not a valid method of {data['name']}\", k);\n"
        '\treturn 0;\n'
        '}\n\n'
    ))

def push_static_functions(typeName, funcList, outFile):
    for funcName, funcData in funcList.items():
        nativeFuncName = Codegen.get_function_wrapper_name(typeName, funcName)

        if 'native_lua_function' in funcData:
            nativeFuncName = funcData['native_lua_function']

        outFile.write((
            f"\tlua_pushcfunction(L, {nativeFuncName}, \"{nativeFuncName}\");\n"
            f"\tlua_setfield(L, -2, \"{funcName}\");\n"
        ))

        if 'aliases' in funcData:
            for alias in funcData['aliases']:
                outFile.write((
                    f"\tlua_pushcfunction(L, {nativeFuncName}, \"{nativeFuncName}\");\n"
                    f"\tlua_setfield(L, -2, \"{alias}\");\n"
                ))

def gen_library_load_for_type(data, outFile):
    if not data['constructors'] and not data['functions']:
        return

    functionName = Codegen.format_method_name(data['name']) + '_lua_load'

    outFile.write((
        f"void {functionName}(lua_State* L) " '{\n'
        f"\tluaL_findtable(L, LUA_GLOBALSINDEX, \"{data['name']}\", 0);" '\n\n'
    ))

    push_static_functions(data['name'], data['constructors'], outFile)
    push_static_functions(data['name'], data['functions'], outFile)

    outFile.write((
        '\n\tlua_pop(L, 1);\n'
        '}\n\n'
    ))

def gen_metamethod_init_for_type(data, outFile):
    if 'has_native_pusher' in data and data['has_native_pusher']:
        return

    metamethods = []

    indexFunctionName = Codegen.format_method_name(data['name']) + '_lua_index'
    namecallFunctionName = Codegen.format_method_name(data['name']) + '_lua_namecall'

    outFile.write((
        f"void LuaPusher<{data['name']}>::init_metatable(lua_State* L) " '{\n'
    ))

    metamethods.append((
        f"\t\tlua_pushstring(L, \"{data['name']}\");\n"
        '\t\tlua_setfield(L, -2, "__type");\n'
    ))

    if data['properties']:
        metamethods.append((
            f"\t\tlua_pushcfunction(L, {indexFunctionName}, \"{indexFunctionName}\");\n"
            '\t\tlua_setfield(L, -2, "__index");\n'
        ))

    if data['methods']:
        metamethods.append((
            f"\t\tlua_pushcfunction(L, {namecallFunctionName}, \"{namecallFunctionName}\");\n"
            '\t\tlua_setfield(L, -2, "__namecall");\n'
        ))

    if 'metamethods' in data and data['metamethods']:
        for funcName, funcData in data['metamethods'].items():
            nativeFuncName = Codegen.get_function_wrapper_name(data['name'], funcName)

            if 'native_lua_function' in funcData:
                nativeFuncName = funcData['native_lua_function']

            metamethods.append((
                f"\t\tlua_pushcfunction(L, {nativeFuncName}, \"{nativeFuncName}\");\n"
                f"\t\tlua_setfield(L, -2, \"{funcName}\");\n"
            ))

    outFile.write(f"\tif (luaL_newmetatable(L, \"{data['name']}\")) " '{\n')

    metamethods.append('\t\tlua_setreadonly(L, -1, true);\n')
    outFile.write('\n'.join(metamethods))

    outFile.write((
        '\t}\n\n'
        '\tlua_setmetatable(L, -2);\n'
        '}\n\n'
    ))

def gen_push_for_type(data, outFile):
    if 'has_native_pusher' in data and data['has_native_pusher']:
        return

    outFile.write((
        'template <>\n'
        f"struct LuaPusher<{data['name']}> " '{\n'
        '\tvoid init_metatable(lua_State* L);\n\n'
    ))

    if Codegen.is_vector(data):
        outFile.write((
            '\tvoid operator()(lua_State* L, float x, float y, float z) {\n'
            '\t\tlua_pushvector(L, x, y, z);\n'
            '\t\tinit_metatable(L);\n'
            '\t}\n\n'
            f"\tvoid operator()(lua_State* L, const {data['name']}& v)" '{\n'
            '\t\treturn operator()(L, v[0], v[1], v[2]);\n'
            '\t}\n'
        ))
    else:
        outFile.write((
            '\ttemplate <typename... Args>\n'
            f"\t{data['name']}* operator()(lua_State* L, Args&&... args) " '{\n'
            f"\t\tauto* pObj = reinterpret_cast<{data['name']}*>(lua_newuserdatatagged(L, sizeof({data['name']}), LuaTypeTraits<{data['name']}>::TAG));\n"
            f"\t\tpObj = std::construct_at<{data['name']}>(pObj, std::forward<Args>(args)...);\n\n"
            '\t\tif (!pObj) [[unlikely]] {\n'
            '\t\t\treturn nullptr;\n'
            '\t\t}\n\n'
            '\t\tinit_metatable(L);\n\n'
            '\t\treturn pObj;\n'
            '\t}\n'
        ))

    outFile.write('};\n\n')

def gen_code_for_types(outFile):
    for data in codegen.typeDataByName.values():
        gen_function_wrappers_for_type(data, outFile)
        gen_index_for_type(data, outFile)
        gen_namecall_for_type(data, outFile)
        gen_library_load_for_type(data, outFile)
        gen_metamethod_init_for_type(data, outFile)

def gen_source_file(outFile):
    outFile.write((
        '#include <script_env.hpp>\n'
        '#include <script_common.hpp>\n\n'
        '#include <cstring>\n'
        '#include <lualib.h>\n'
        '#include <glm/geometric.hpp>\n'
        '\n'
    ))

    gen_useratom(outFile)
    gen_code_for_types(outFile)

def gen_header_file(outFile):
    outFile.write((
        '#pragma once\n\n'
        '#include <script_fwd.hpp>\n\n'
        '#include <cstdint>\n\n'
        '#include <string_view>\n\n'
    ))

    outFile.write(codegen.includeBlock)

    for atomName, value in keywordToStringAtom.items():
        atomVarName = 'LUA_ATOM_' + Codegen.format_constant_name(atomName)
        outFile.write(f"constexpr const int16_t {atomVarName} = {value};\n")

    for data in codegen.typeDataByName.values():
        tagValue = codegen.tagValueByName[data['name']]

        outFile.write((
            'template <>\n'
            f"struct LuaTypeTraits<{data['name']}> " '{\n'
            f"\tstatic constexpr const int TAG = {tagValue};\n"
            f"\tstatic constexpr const std::string_view NAME = \"{data['name']}\";\n"
            '};\n\n'
        ))

        gen_push_for_type(data, outFile)

def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} interface_path build_path", file=sys.stderr)
        exit(1)

    parentDir = path.join(sys.argv[2], 'generated')

    if not path.exists(parentDir):
        os.mkdir(parentDir)

    outSourceName = path.join(parentDir, 'lua_bindings.cpp')
    outHeaderName = path.join(parentDir, 'script_common.hpp')

    global idlBasePath
    idlBasePath = sys.argv[1]

    global codegen
    codegen = Codegen(idlBasePath)

    init_keyword_to_string_atom()

    with open(outSourceName, 'w') as outSourceFile, open(outHeaderName, 'w') as outHeaderFile:
        gen_source_file(outSourceFile)
        gen_header_file(outHeaderFile)

if __name__ == "__main__":
    main()
