local game = Farsh.Game.Get()

Farsh.t = {}
local t = Farsh.t

game:SetAmbient(0.02, 0.02, 0.02)

-- материал кровищи
local matBlood = Farsh.Material()
--matBlood:SetDiffuseTexture(game:LoadTexture("/blood.png"))
matBlood:SetDiffuse({1, 0, 0, 1})
matBlood:SetSpecular({0.2, 0, 0, 0})
game:SetDecalMaterial(matBlood)

-- материал кубика
local matCube = Farsh.Material()
t.matCube = matCube
--matCube:SetDiffuseTexture(game:LoadTexture("/blood.png"))
matCube:SetDiffuse({1, 1, 1, 1})
--matCube:SetSpecularTexture(game:LoadTexture("/specular.jpg"))
matCube:SetSpecular({0.2, 0, 0, 0})

local CreateStaticRigidModel = function(geometry, material, physicsShape, x, y, z)
	game:AddStaticModel(geometry, material, x, y, z)
	game:CreatePhysicsRigidBody(physicsShape, 0, x, y, z)
end

local matBench = Farsh.Material()
t.matBench = matBench
matBench:SetDiffuse({1, 1, 1, 1})
matBench:SetSpecular({0.1, 0.1, 0.1, 0.2})
game:AddStaticModel(game:LoadGeometry("/bench.geo"), matBench, 0, 2, 0)

local matNescafe = Farsh.Material()
local texNescafe = game:LoadTexture("/nescafe.png")
matNescafe:SetDiffuseTexture(texNescafe)
matNescafe:SetSpecularTexture(texNescafe)
game:AddStaticModel(game:LoadGeometry("/nescafe.geo"), matNescafe, 2, 0, 0)

local geoCube = game:LoadGeometry("/box.geo")
local shapeCube = game:CreatePhysicsBoxShape(1, 1, 1)
game:CreatePhysicsRigidBody(game:CreatePhysicsBoxShape(10, 10, 1), 0, 11, 11, -1)
for i = 1, 10 do
	for j = 1, 10 do
		game:AddStaticModel(geoCube, matCube, i * 2, j * 2, -1)
	end
end
--[ 8x8x4 (256) ok, 9x9x4 (324) not, 10x10x3 (300) ok, 7x7x5 (245) ok, 8x8x5 (320) not
for i = 1, 4 do
for j = 1, 4 do
	for k = 1, 5 do
		game:AddRigidModel(geoCube, matCube, game:CreatePhysicsRigidBody(shapeCube, 100, i * 2 + k, j * 2 + k, k * 4 + 20))
		--game:AddRigidModel(geoCube, matCube, game:CreatePhysicsRigidBody(shapeCube, 100, i * 2, i * 2 + 1, 13))
		--game:AddRigidModel(geoCube, matCube, game:CreatePhysicsRigidBody(shapeCube, 100, i * 2, i * 2 - 1, 13))
	end
end
end
--]]
--[[
local light1 = game:AddStaticLight()
light1:SetPosition(10, 10, 5)
light1:SetTarget(9, 9, 1)
light1:SetShadow(true)
--]]
local light2 = game:AddStaticLight()
light2:SetPosition({30, -10, 20})
light2:SetTarget({11, 11, 0})
light2:SetProjection(45, 0.1, 100)
light2:SetShadow(true)

-- установка параметров

local zhDiffuse = game:LoadTexture("/zombie_d.png")
local zhSpecular = game:LoadTexture("/zombie_s.png")

local zombieMaterial = Farsh.Material()
zombieMaterial:SetDiffuseTexture(zhDiffuse)
zombieMaterial:SetSpecularTexture(zhSpecular)
local zombieGeometry = game:LoadSkinnedGeometry("/zombie.geo")
local zombieSkeleton = game:LoadSkeleton("/zombie.skeleton")
game:SetZombieParams(zombieMaterial, zombieGeometry, zombieSkeleton, game:LoadBoneAnimation("/zombie.ba", zombieSkeleton))

game:SetHeroParams(zombieMaterial, zombieGeometry, zombieSkeleton, game:LoadBoneAnimation("/hero.ba", zombieSkeleton))

local axeMaterial = Farsh.Material()
axeMaterial:SetDiffuseTexture(game:LoadTexture("/axe_d.png"))
--axeMaterial:SetSpecularTexture(game:LoadTexture("axe_s.png"))
axeMaterial:SetSpecular({0.5, 0, 0, 0})
game:SetAxeParams(axeMaterial, game:LoadGeometry("/axe.geo"), game:LoadBoneAnimation("/axe.ba", nil))

local circularMaterial = Farsh.Material()
circularMaterial:SetDiffuseTexture(game:LoadTexture("/circular_d.png"))
circularMaterial:SetSpecularTexture(game:LoadTexture("/circular_s.png"))
game:SetCircularParams(circularMaterial, game:LoadGeometry("/circular.geo"), game:LoadBoneAnimation("/circular.ba", nil))

game:PlaceHero(10, 10, 10)
