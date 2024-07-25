import sys
import os
from os import path
from codegen import Codegen

keywordToStringAtom = dict()
stringAtomCounter = 0

def init_string_atoms_for_list(nameList):
    global stringAtomCounter

    for key in nameList:
        if not key in keywordToStringAtom:
            keywordToStringAtom[key] = stringAtomCounter
            stringAtomCounter += 1

def init_keyword_to_string_atom():
    for name, data in codegen.typeDataByName.items():
        filteredProperties = [key for key, value in data['properties'].items() if 'skip_lua_codegen' not in value or not value['skip_lua_codegen']]

        init_string_atoms_for_list(filteredProperties)
        init_string_atoms_for_list(data['functions'].keys())
        init_string_atoms_for_list(data['methods'].keys())
        init_string_atoms_for_list(data['events'].keys())

def gen_useratom(outFile):
    pairs = []

    for atomName, value in keywordToStringAtom.items():
        pairs.append((
            f"if (strncmp(s, \"{atomName}\", l) == 0) " '{\n'
            '\t\t' f"return {value};" '\n'
            '\t}'
        ))

    outFile.write((
        '\nint16_t ScriptEnvironment::useratom(const char* s, size_t l) {\n\t'
    ))

    outFile.write("\n\telse ".join(pairs))

    outFile.write((
        '\n\n\treturn -1;\n'
        '}\n\n'
    ))

def gen_function_wrapper_for_type(data, outFile, funcName, funcData, isMethod, isConstructor):
    # Free functions get wrapped
    if 'native_free_function' in funcData or isConstructor:
        wrapperFunctionName = data['name'].lower() + '_lua_' + funcName.lower() + '_wrapper'

        outFile.write(f"static int {wrapperFunctionName}(lua_State* L) " '{\n')

        args = []
        ptrCheckArgs = []

        if isMethod:
            selfCheckExpr = Codegen.get_get_expression(data['name'], 'self', 1)
            outFile.write(f"\t{selfCheckExpr}\n")
            args.append('*self')

        for i in range(len(funcData['parameters'])):
            paramData = funcData['parameters'][i]
            stackIndex = i + 2 if isMethod else i + 1

            if 'default' in paramData:
                optExpr = Codegen.get_optional_expression(paramData['type'], paramData['name'], stackIndex,
                        paramData['default'])
                outFile.write(f"\t{optExpr}\n")
                args.append(paramData['name'])
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
            getterExpr = funcData['native_free_function'] + '(' + getterExpr + ')'

        returnType = data['name'] if isConstructor else funcData['return_types'][0]
        pushExpr = Codegen.get_push_expression(returnType, getterExpr)

        outFile.write('\n\t')
        outFile.write(pushExpr)
        outFile.write(';\n\treturn 1;\n')
        outFile.write('}\n\n')

def gen_function_wrappers_for_type(data, outFile):
    for methodName, methodData in data['methods'].items():
        gen_function_wrapper_for_type(data, outFile, methodName, methodData, True, False)

    for funcName, funcData in data['functions'].items():
        gen_function_wrapper_for_type(data, outFile, funcName, funcData, False, False)

    for funcName, funcData in data['constructors'].items():
        gen_function_wrapper_for_type(data, outFile, funcName, funcData, False, True)

def gen_index_for_type(data, outFile):
    functionName = data['name'].lower() + '_lua_index'
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

        atom = keywordToStringAtom[propName]
        getterExpr = Codegen.get_getter_expression(data, propName, propData)
        pushExpr = Codegen.get_push_expression(propData['type'], getterExpr)

        cases.append((
            f"\t\tcase {atom}: // {propName}\n"
            f"\t\t\t{pushExpr};\n"
            '\t\t\treturn 1;\n'
        ))

    outFile.write(''.join(cases))

    outFile.write((
        '\t\tdefault:\n'
        '\t\t\tbreak;\n'
        '\t}\n\n'
        '\treturn 0;\n'
        '}\n\n'
    ))

def gen_namecall_for_type(data, outFile):
    functionName = data['name'].lower() + '_lua_namecall'

    outFile.write((
        f"int {functionName}(lua_State* L) " '{\n\t'
        '\n\tint atom;\n'
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

        atom = keywordToStringAtom[methodName]
        wrapperFunctionName = data['name'].lower() + '_lua_' + methodName.lower() + '_wrapper'

        cases.append((
            f"\t\tcase {atom}: // {methodName}\n"
            f"\t\t\treturn {wrapperFunctionName}(L);\n"
            '\t\t\treturn 1;\n'
        ))

    outFile.write(''.join(cases))

    outFile.write((
        '\t\tdefault:\n'
        '\t\t\tbreak;\n'
        '\t}\n\n'
        '\treturn 0;\n'
        '}\n\n'
    ))

def gen_library_load_for_type(data, outFile):
    functionName = data['name'].lower() + '_lua_load'

    outFile.write((
        f"void {functionName}(lua_State* L) " '{\n'
        f"\tluaL_findtable(L, LUA_GLOBALSINDEX, \"{data['name']}\", 0);" '\n\n'
    ))

    for funcName in data['constructors'].keys():
        nativeFuncName = data['name'].lower() + '_lua_' + funcName.lower() + '_wrapper'
        outFile.write((
            f"\tlua_pushcfunction(L, {nativeFuncName}, \"{nativeFuncName}\");\n"
            f"\tlua_setfield(L, -2, \"{funcName}\");\n"
        ))

    for funcName in data['functions'].keys():
        nativeFuncName = data['name'].lower() + '_lua_' + funcName.lower() + '_wrapper'
        outFile.write((
            f"\tlua_pushcfunction(L, {nativeFuncName}, \"{nativeFuncName}\");\n"
            f"\tlua_setfield(L, -2, \"{funcName}\");\n"
        ))

    outFile.write((
        '\n\tlua_pop(L, 1);\n'
        '}\n\n'
    ))

def gen_code_for_types(outFile):
    for data in codegen.typeDataByName.values():
        gen_function_wrappers_for_type(data, outFile)
        gen_index_for_type(data, outFile)
        gen_namecall_for_type(data, outFile)
        gen_library_load_for_type(data, outFile)

def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} interface_path build_path", file=sys.stderr)
        exit(1)

    parentDir = path.join(sys.argv[2], 'generated')

    if not path.exists(parentDir):
        os.mkdir(parentDir)

    outSourceName = path.join(parentDir, 'lua_bindings.cpp')
    global idlBasePath
    idlBasePath = sys.argv[1]

    global codegen
    codegen = Codegen(idlBasePath)

    init_keyword_to_string_atom()

    with open(outSourceName, 'w') as outFile:
        outFile.write((
            '#include <script_env.hpp>\n\n'
            '#include <cstring>\n'
            '#include <lualib.h>\n'
            '#include <glm/geometric.hpp>\n'
            '\n'
        ))

        for data in codegen.typeDataByFile.values():
            outFile.write(f"#include {data['native_include']}\n")

        gen_useratom(outFile)
        gen_code_for_types(outFile)

if __name__ == "__main__":
    main()
