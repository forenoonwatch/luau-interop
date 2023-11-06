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

print("[do_thing] spawning coro")
local coro = coroutine.create(do_thing)
local succ, res = coroutine.resume(coro, 3, 2)
print(succ, res)

