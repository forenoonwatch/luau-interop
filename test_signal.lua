local c; c = event:Connect(function(v1, v2, vs)
	c:Disconnect()
	print("Connect:", v1, v2, vs)
end)

local cc = event:Once(function(...)
	print("Once: ", ...)
end)

local c3 = event:Connect(function(...)
	print("Connect3:", ...)
end)

print("Wait:", event:Wait())

print("Done running this")
