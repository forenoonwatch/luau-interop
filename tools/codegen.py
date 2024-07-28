import json
import os
from os import path
import re

class Codegen:
    def __init__(self, idlBasePath):
        self.idlBasePath = idlBasePath
        self.typeDataByFile = dict()
        self.typeDataByName = dict()
        self.tagValueByName = dict()

        self.init_type_data()
        self.init_tags()
        self.init_include_block()

    def init_type_data(self):
        typePath = path.join(self.idlBasePath, 'types')

        for fileName in os.listdir(typePath):
            fullFileName = path.join(typePath, fileName)

            if path.isfile(fullFileName):
                with open(fullFileName, 'r') as inFile:
                    data = json.load(inFile)
                    self.typeDataByFile[fileName] = data
                    self.typeDataByName[data['name']] = data

    def init_tags(self):
        types = []

        for name in self.typeDataByName.keys():
            types.append(name)

        types.sort()

        for value, name in enumerate(types):
            self.tagValueByName[name] = value + 1

    def init_include_block(self):
        includeSet = set()

        for data in self.typeDataByName.values():
            includeSet.add(data['native_include'])

        self.includeBlock = '#include ' + '\n#include '.join(includeSet) + '\n'

    def apply_name_format(name):
        fmt = '_'.join([v.group() for v in re.finditer(r'[A-Z]*(\d+[A-Z]?|[a-z]+)+', name)])
        return fmt if fmt else name

    def format_method_name(name):
        return Codegen.apply_name_format(name).lower()

    def format_constant_name(name):
        return Codegen.apply_name_format(name).upper()

    def is_core_type(typeName):
        return typeName == 'float' or typeName == 'bool'

    def is_vector(data):
        return 'is_lua_vector' in data and data['is_lua_vector']

    def get_function_wrapper_name(typeName, funcName):
        return Codegen.format_method_name(typeName) + '_lua_' + funcName.lower().replace('__', '') + '_wrapper'

    def get_push_expression(typeName, args):
        if typeName == 'float':
            return f"lua_pushnumber(L, {args})"
        elif typeName == 'bool':
            return f"lua_pushboolean(L, {args})"
        else:
            return f"lua_push<{typeName}>(L, {args})" if args else f"lua_push<{typeName}>(L)"

    def get_check_expression(typeName, varName, stackPosition):
        if typeName == 'float':
            return f"float {varName} = static_cast<float>(luaL_checknumber(L, {stackPosition}));"
        elif typeName == 'bool':
            return f"bool {varName} = luaL_checkboolean(L, {stackPosition});"
        else:
            return f"const {typeName}* {varName} = lua_check<{typeName}>(L, {stackPosition});"

    def get_optional_expression(typeName, varName, stackPosition, defaultValue):
        if typeName == 'float':
            return f"float {varName} = static_cast<float>(luaL_optnumber(L, {stackPosition}, {defaultValue}));"
        elif typeName == 'bool':
            return f"bool {varName} = luaL_optboolean(L, {stackPosition}, {defaultValue});"
        else:
            return f"const {typeName}* {varName} = lua_opt<{typeName}>(L, {stackPosition}, {defaultValue});"

    def get_native_getter_name(propName, propData):
        return propData['native_getter'] if 'native_getter' in propData else 'get_' + Codegen.format_method_name(propName)

    def get_getter_expression(data, propName, propData):
        getterName = Codegen.get_native_getter_name(propName, propData)

        if Codegen.is_vector(data):
            return f"{getterName}(*obj)"
        else:
            return f"obj->{getterName}"

