#include "stdafx.h"
#include "MenuState.h"
#include "../GameObject.h"
#include "../Components/Render_Components/LayerCP.h"
#include "../Manager/RenderManager.h"
#include "../Manager/AssetManager.h"
#include "../Components/Graphics_Components/AnimatedGraphicsCP.h"
#include "../Components/Transformation_Components/TransformationCP.h"
#include "../Components/Input_Components/MovementInputArrowsCP.h"
#include "../Components/Input_Components/MovementInputWASDCP.h"
#include "../Components/Collision_Components/RectCollisionCP.h"
#include "../Components/Render_Components/SpriteRenderCP.h"
#include "../DebugDraw.h"

void MenuState::init(sf::RenderWindow& rWindow)
{
	this->window.reset(&rWindow, [](sf::RenderWindow*) {});

	this->window->setSize(sf::Vector2u(975, 650));

	DebugDraw::getInstance().initialize(*window);

	spriteSheetCounts["Player1"] = { 1, 1, 1, 1, 4, 4, 4, 4 };
	spriteSheetCounts["Player2"] = { 1, 1, 1, 1, 4, 4, 4, 4 };

	loadMap("game.tmj", sf::Vector2f());
	
	currentLayer = 0;
	std::cout << "Current Layer: " << currentLayer << std::endl;

	for (auto& go : gameObjects)
	{
		go->init();
		
		for (auto& renderCPs : go->getComponentsOfType<RenderCP>()) 
		{
			RenderManager::getInstance().addToLayers(renderCPs);
		}
	}
	
	window->setView(sf::View(sf::Vector2f(window->getSize().x / 2, window->getSize().y / 2), (sf::Vector2f)window->getSize()));
}

void MenuState::exit()
{
	for (auto& go : gameObjects)
	{
		go.reset();
	}
	gameObjects.clear();

	for (auto& comp : RenderManager::getInstance().getLayers())
	{
		comp.reset();
	}

	RenderManager::getInstance().getLayers().clear();
	maxLayer = 0;
	DebugDraw::getInstance().unload();
}

void MenuState::update(float deltaTime)
{
	for (auto& go : gameObjects)
	{
		go->update(deltaTime);
	}

	checkAreaBorders();

	checkPlayerLayer();
}

void MenuState::render()
{
	RenderManager::getInstance().render();
}

void MenuState::loadMap(std::string name, const sf::Vector2f& offset)
{
	tson::Tileson t;
	const std::unique_ptr<tson::Map> map = t.parse(m_resourcePath / name);
	std::shared_ptr<GameObject> mapGO = std::make_shared<GameObject>("Map");

	if (map->getStatus() == tson::ParseStatus::OK)
	{

		sf::err() << "Load tile map with size: " << map->getSize().x << ", "
			<< map->getSize().y << " and tile size: " << map->getTileSize().x
			<< ", " << map->getTileSize().y << std::endl;

		for (auto& tileSet : map->getTilesets())
		{
			fs::path tileSetPath = tileSet.getImage().u8string();

			const auto texture = std::make_shared<sf::Texture>();
			if (!texture->loadFromFile((m_resourcePath / tileSetPath).string()))
			{
				sf::err() << "Could not load texture " << m_resourcePath / tileSetPath << std::endl;
			}
			m_tileSetTexture[tileSet.getName()] = texture;
		}
	}
	else
	{
		std::cout << "Parse error: " << map->getStatusMessage() << std::endl;
	}

	// go through all layers

	std::shared_ptr<LayerCP> layerCP;

	for(auto& layer : map->getLayers())
	{
		sf::err() << "Load layer: " << layer.getName() << " with width: "
			<< layer.getSize().x << " and height: " << layer.getSize().y << std::endl;

		const int size = layer.getSize().x * layer.getSize().y;

		int layerNr = layer.getProp("LayerNr")->getValue<int>();

		layerCP = std::make_shared<LayerCP>(mapGO, "Layer" + layerNr, window, layerNr, std::vector<std::shared_ptr<sf::Sprite>>());
		
		// iterate over all elements/tiles in the layer
		for (int i = 0; i < size; i++)
		{
			const auto gid = layer.getData()[i];

			if (gid == 0)
			{
				// 0 means there is no tile at this position.
				continue;
			}

			// get tile set for tile and allocate the corresponding tileSet texture
			const auto* const tileSet = map->getTilesetByGid(gid);
			const sf::Vector2i tileSize(map->getTileSize().x, map->getTileSize().y);
			sf::Texture& texture = *m_tileSetTexture[tileSet->getName()];
			
			// calculate position of tile
			sf::Vector2f position;
			position.x = i % layer.getSize().x * static_cast<float>(tileSize.x);
			position.y = i / layer.getSize().x * static_cast<float>(tileSize.y);
			position += offset;

			// number of tiles in tile set texture (horizontally)
			const int tileCountX = texture.getSize().x / tileSize.x;

			// calculate 2d idx of tile in tile set texture
			const int idx = gid - tileSet->getFirstgid();
			const int idxX = idx % tileCountX;
			const int idxY = idx / tileCountX;

			// calculate source area of tile in tile set texture
			sf::IntRect source(idxX * tileSize.x, idxY * tileSize.y, tileSize.x, tileSize.y);	

			// create tile (Sprite) and put it into the appropriate layer.
			auto sprite = std::make_shared<sf::Sprite>();
			sprite->setTexture(texture);
			sprite->setTextureRect(source);
			sprite->setPosition(position.x, position.y);

			layerCP->getLayer().push_back(sprite);

		}
		mapGO->addComponent(layerCP);
		maxLayer++;
	}

	this->gameObjects.push_back(mapGO);

	for (auto group : map->getLayers())
	{
		// go over all objects per layer
		for (auto object : group.getObjects())
		{
			sf::Vector2f position(object.getPosition().x, object.getPosition().y);
			position += offset;

			// example to get a texture rectangle for a sprite
			sf::FloatRect bounds(position.x, position.y, object.getSize().x, object.getSize().y);
			// TODO: check out game.tmj and there the content contained within <object group name="Object Layer 1">
			// there you can see the properties that you can parse, that should help you set up the sprites
			std::cout << object.getRotation() << std::endl;
			std::cout << object.getType() << std::endl;
			
			if (object.getProp("ObjectGroup")->getValue<std::string>() == "Player")
			{
				createPlayers(object, group);
			}
		}
	}
}

void MenuState::checkAreaBorders()
{
	auto left = window->getView().getCenter().x - window->getView().getSize().x / 2;
	auto top = window->getView().getCenter().y - window->getView().getSize().y / 2;
	auto right = window->getView().getCenter().x + window->getView().getSize().x / 2;
	auto bottom = window->getView().getCenter().y + window->getView().getSize().y / 2;

	std::shared_ptr<TransformationCP> transf;
	std::shared_ptr<RectCollisionCP> collision;

	for (auto& go : gameObjects)
	{
		//Check for both players
		if ((transf = std::dynamic_pointer_cast<TransformationCP>(go->getComponent("PlayerTransformationCP"))) && (collision = std::dynamic_pointer_cast<RectCollisionCP>(go->getComponent("PlayerCollisionCP"))))
		{
			if (transf && collision)
			{
				if (transf->getPosition().y > bottom - collision->getCollisionRect().height / 2)
					transf->setPosition(sf::Vector2f(transf->getPosition().x, bottom - collision->getCollisionRect().height / 2));

				if (transf->getPosition().y < top + collision->getCollisionRect().height / 2)
					transf->setPosition(sf::Vector2f(transf->getPosition().x, top + collision->getCollisionRect().height / 2));

				if (transf->getPosition().x > right - collision->getCollisionRect().width / 2)
					transf->setPosition(sf::Vector2f(right - collision->getCollisionRect().width / 2, transf->getPosition().y));

				if (transf->getPosition().x < left + collision->getCollisionRect().width / 2)
					transf->setPosition(sf::Vector2f(left + collision->getCollisionRect().width / 2, transf->getPosition().y));
			}
		}
	}
}

void MenuState::checkPlayerLayer()
{

	if (InputManager::getInstance().getKeyDown(sf::Keyboard::Home) || InputManager::getInstance().getKeyDown(sf::Keyboard::End))
	{
		if (InputManager::getInstance().getKeyDown(sf::Keyboard::Home) && currentLayer < maxLayer)
		{
			currentLayer++;
			std::cout << "Current Layer: " << currentLayer << std::endl;
		}
		else if(currentLayer > 0)
		{
			currentLayer--;
			std::cout << "Current Layer: " << currentLayer << std::endl;
		}
	}

	if (InputManager::getInstance().getKeyDown(sf::Keyboard::PageUp) || InputManager::getInstance().getKeyDown(sf::Keyboard::PageDown))
	{
		if (InputManager::getInstance().getKeyDown(sf::Keyboard::PageUp) && currentLayer < maxLayer)
		{
			for (auto& cp : RenderManager::getInstance().getLayers())
			{
				if (cp->getLayerNr() == currentLayer)
				{
					cp->setLayerNr(currentLayer + 1);
				}
				else if (cp->getLayerNr() == currentLayer + 1)
				{
					cp->setLayerNr(currentLayer);
				}
			}
			currentLayer++;
			std::cout << "New current Layer: " << currentLayer << std::endl;
		}
		else if (InputManager::getInstance().getKeyDown(sf::Keyboard::PageDown) && currentLayer > 0)
		{
			for (auto& cp : RenderManager::getInstance().getLayers())
			{
				if (cp->getLayerNr() == currentLayer)
				{
					cp->setLayerNr(currentLayer - 1);
				}
				else if (cp->getLayerNr() == currentLayer - 1)
				{
					cp->setLayerNr(currentLayer);
				}
			}
			currentLayer--;
			std::cout << "New current Layer: " << currentLayer << std::endl;
		}
	}
}

void MenuState::createPlayers(tson::Object& object, tson::Layer group)
{
	int idNr = object.getProp("PlayerNr")->getValue<int>();
	std::string stringId = "Player";
	stringId += '0' + idNr;

	std::shared_ptr<GameObject> playerTemp = std::make_shared<GameObject>(stringId);

	if (!AssetManager::getInstance().Textures["PlayerTexture"])
	{
		AssetManager::getInstance().loadTexture("PlayerTexture", object.getProp("PlayerTexture")->getValue<std::string>());
	}

	const int PLAYER_ANIMATION_SPEED = object.getProp("AnimationSpeed")->getValue<int>();

	std::shared_ptr<AnimatedGraphicsCP> playerGraphicsCP = std::make_shared<AnimatedGraphicsCP>(
		playerTemp, "PlayerSpriteCP", *AssetManager::getInstance().Textures.at("PlayerTexture"), spriteSheetCounts[playerTemp->getId()], PLAYER_ANIMATION_SPEED
	);

	playerTemp->addComponent(playerGraphicsCP);

	const float VELOCITY = object.getProp("Velocity")->getValue<int>();
	sf::Vector2f pos(sf::Vector2f(object.getPosition().x, object.getPosition().y));

	std::shared_ptr<TransformationCP> transCP = std::make_shared<TransformationCP>(playerTemp, "PlayerTransformationCP", pos, object.getRotation(), object.getSize().x);
	transCP->setVelocity(VELOCITY);

	playerTemp->addComponent(transCP);

	bool useArrowKeys = object.getProp("ArrowKeys")->getValue<bool>();

	if (useArrowKeys) {
		std::shared_ptr<MovementInputArrowsCP> movementInputCP = std::make_shared<MovementInputArrowsCP>(
			playerTemp, "MovementInputCP", playerGraphicsCP, transCP
		);
		playerTemp->addComponent(movementInputCP);
	}
	else {
		std::shared_ptr<MovementInputWASDCP> movementInputCP = std::make_shared<MovementInputWASDCP>(
			playerTemp, "MovementInputCP", playerGraphicsCP, transCP
		);
		playerTemp->addComponent(movementInputCP);
	}

	std::shared_ptr<RectCollisionCP> playerCollisionCP = std::make_shared<RectCollisionCP>(playerTemp, "PlayerCollisionCP");
	playerTemp->addComponent(playerCollisionCP);

	std::shared_ptr<SpriteRenderCP> playerRenderCP = std::make_shared<SpriteRenderCP>(playerTemp, "PlayerRenderCP", window, group.getProp("LayerNr")->getValue<int>());
	playerTemp->addComponent(playerRenderCP);

	gameObjects.push_back(playerTemp);

}