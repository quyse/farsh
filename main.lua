local game = Farsh.Game.Get()

Farsh.t = {}
local t = Farsh.t

-- материал кубика
local matCube = Farsh.Material()
t.matCube = matCube
matCube:SetDiffuseTexture(game:LoadTexture("diffuse.jpg"))
--matCube:SetSpecularTexture(game:LoadTexture("specular.jpg"))
matCube:SetSpecular(0.2, 0, 0, 0)

local CreateStaticRigidModel = function(geometry, material, physicsShape, x, y, z)
	game:AddStaticModel(geometry, material, x, y, z)
	game:CreatePhysicsRigidBody(physicsShape, 0, x, y, z)
end

local matBench = Farsh.Material()
t.matBench = matBench
matBench:SetDiffuse(1, 1, 1, 1)
matBench:SetSpecular(0.1, 0.1, 0.1, 0.2)
game:AddStaticModel(game:LoadGeometry("bench.geo"), matBench, 0, 2, 0)

local matNescafe = Farsh.Material()
local texNescafe = game:LoadTexture("nescafe.png")
matNescafe:SetDiffuseTexture(texNescafe)
matNescafe:SetSpecularTexture(texNescafe)
game:AddStaticModel(game:LoadGeometry("nescafe.geo"), matNescafe, 2, 0, 0)

local geoCube = game:LoadGeometry("box.geo")
local shapeCube = game:CreatePhysicsBoxShape(1, 1, 1)
game:CreatePhysicsRigidBody(game:CreatePhysicsBoxShape(10, 10, 1), 0, 11, 11, -1)
for i = 1, 10 do
	for j = 1, 10 do
		game:AddStaticModel(geoCube, matCube, i * 2, j * 2, -1)
	end
end
for i = 1, 10 do
	game:AddRigidModel(geoCube, matCube, game:CreatePhysicsRigidBody(shapeCube, 100, i * 2, i * 2, 10))
	game:AddRigidModel(geoCube, matCube, game:CreatePhysicsRigidBody(shapeCube, 100, i * 2, i * 2 + 1, 13))
	game:AddRigidModel(geoCube, matCube, game:CreatePhysicsRigidBody(shapeCube, 100, i * 2, i * 2 - 1, 13))
end
local light1 = game:AddStaticLight()
light1:SetPosition(10, 10, 5)
light1:SetTarget(9, 9, 1)
light1:SetShadow(true)
local light2 = game:AddStaticLight()
light2:SetPosition(30, -10, 20)
light2:SetTarget(11, 11, 0)
light2:SetProjection(45, 0.1, 100)
light2:SetShadow(true)

--local geoZombie = game:LoadGeometry("zombie.geo")

--game:SetupZombie(geoZombie, matZombie)
