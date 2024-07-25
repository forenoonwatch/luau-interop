import json
import os
from os import path
import re

class Codegen:
    def __init__(self, idlBasePath):
        self.idlBasePath = idlBasePath
        self.typeDataByFile = dict()
        self.typeDataByName = dict()

        self.init_type_data()

    def init_type_data(self):
        typePath = path.join(self.idlBasePath, 'types')

        for fileName in os.listdir(typePath):
            fullFileName = path.join(typePath, fileName)

            if path.isfile(fullFileName):
                with open(fullFileName, 'r') as inFile:
                    data = json.load(inFile)
                    self.typeDataByFile[fileName] = data
                    self.typeDataByName[data['name']] = data

    def format_method_name(name):
        return '_'.join(re.findall('([A-Z][a-z\d]*)', name)).lower()

    def is_core_type(typeName):
        return typeName == 'float' or typeName == 'bool'

    def is_vector(data):
        return 'is_lua_vector' in data and data['is_lua_vector']

    def get_push_expression(typeName, args):
        if typeName == 'float':
            return f"lua_pushnumber(L, {args})"
        elif typeName == 'bool':
            return f"lua_pushboolean(L, {args})"
        else:
            funcName = typeName.lower() + '_lua_push'
            return f"{funcName}(L, {args})"

    def get_check_expression(typeName, varName, stackPosition):
        if typeName == 'float':
            return f"float {varName} = static_cast<float>(luaL_checknumber(L, {stackPosition}));"
        elif typeName == 'bool':
            return f"bool {varName} = luaL_checkboolean(L, {stackPosition});"
        else:
            funcName = typeName.lower() + '_lua_check'
            return f"const {typeName}* {varName} = {funcName}(L, {stackPosition});"

    def get_get_expression(typeName, varName, stackPosition):
        if typeName == 'float':
            return f"float {varName} = static_cast<float>(lua_tonumber(L, {stackPosition}));"
        elif typeName == 'bool':
            return f"bool {varName} = lua_toboolean(L, {stackPosition});"
        else:
            funcName = typeName.lower() + '_lua_get'
            return f"const {typeName}* {varName} = {funcName}(L, {stackPosition});"

    def get_optional_expression(typeName, varName, stackPosition, defaultValue):
        if typeName == 'float':
            return f"float {varName} = static_cast<float>(luaL_optnumber(L, {stackPosition}, {defaultValue}));"
        elif typeName == 'bool':
            return f"bool {varName} = luaL_optboolean(L, {stackPosition}, {defaultValue});"
        else:
            funcName = typeName.lower() + '_lua_opt'
            return f"{typeName} {varName} = {funcName}(L, {stackPosition}, {defaultValue});"

    def get_native_getter_name(propName, propData):
        return propData['native_getter'] if 'native_getter' in propData else 'get_' + Codegen.format_method_name(propName)

    def get_getter_expression(data, propName, propData):
        getterName = Codegen.get_native_getter_name(propName, propData)

        if Codegen.is_vector(data):
            return f"{getterName}(*obj)"
        else:
            return f"obj->{getterName}()"

