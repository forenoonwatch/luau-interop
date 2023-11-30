local function do_thing(a, b)
	local c = a + b
	print "[do_thing] thinking about it"
	wait(0.5)
	print "[do_thing] thought about it"

	return c
end

event:Connect(function()
	print("This is a callback")
end)

local mod = require("test_module")
print("From module", mod.Value)

print("Vector3.new is", typeof(Vector3.new(1, 2, 3)))
print(Vector3.new(1, 2, 3) + Vector3.new(3, 2, 1))
print(Vector3.new(3, 4, 0).Magnitude)
print(Vector3.new(1, 0, 0):Cross(Vector3.new(0, 1, 0)))
print(Vector3.new(1, 0, 0):Lerp(Vector3.new(0, 1, 0), 0.5))
print(Vector3.new(1, 0, 0):FuzzyEq(Vector3.new(1, 0, 0)))

local cf = CFrame.fromAxisAngle(Vector3.new(1, 0, 0), 0.5 * math.pi)
print("CFrame", cf)

print('[getfenv] printing getfenv')
table.foreach(getfenv(1), print)

wait = nil

--wait(0.5)

--[[function math.lol()
	print("lol")
end]]

print("HEllo world")

local inst = Instance.new("Part")
print(inst)
print("inst.Name = " .. inst.Name)
inst.Name = "MyInstance"
print("inst.Name = " .. inst.Name)

print("inst.ClassName = " .. inst.ClassName)

for i = 1, 10 do
	local child = Instance.new("Part")
	child.Name = "MyChild" .. i
	child.Parent = inst
end

for _, v in pairs(inst:GetChildren()) do
	print(v)
end

local function temp()
	local itemp = Instance.new("Part")
	itemp.Name = "TempInstance"
end

temp()

collectgarbage()

print("[do_thing] spawning coro")
local coro = coroutine.create(do_thing)
local succ, res = coroutine.resume(coro, 3, 2)
print(succ, res)

