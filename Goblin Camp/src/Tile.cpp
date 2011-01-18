/* Copyright 2010 Ilkka Halila
This file is part of Goblin Camp.

Goblin Camp is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Goblin Camp is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License 
along with Goblin Camp. If not, see <http://www.gnu.org/licenses/>.*/
#include "stdafx.hpp"

#include <string>
#include <queue>
#include <set>
#ifdef DEBUG
#include <iostream>
#endif

#include "Random.hpp"
#include "Tile.hpp"
#include "Announce.hpp"
#include "Logger.hpp"
#include "Game.hpp"
#include "Construction.hpp"

Tile::Tile(TileType newType, int newCost) :
	vis(true),
	walkable(true),
	buildable(true),
	moveCost(newCost),
	construction(-1),
	low(false),
	blocksWater(false),
	water(boost::shared_ptr<WaterNode>()),
	graphic('.'),
	foreColor(TCODColor::white),
	originalForeColor(TCODColor::white),
	backColor(TCODColor::black),
	natureObject(-1),
	npcList(std::set<int>()),
	itemList(std::set<int>()),
	filth(boost::shared_ptr<FilthNode>()),
	blood(boost::shared_ptr<BloodNode>()),
	marked(false),
	walkedOver(0),
	corruption(0),
	territory(false)
{
	SetType(newType);
}

TileType Tile::GetType() { return type; }

void Tile::SetType(TileType newType) {
	type = newType;
	if (type == TILEGRASS) {
		vis = true; walkable = true; buildable = true;
		originalForeColor = TCODColor(Random::Generate(49), 127, 0);
		backColor = TCODColor(0, 0, 0);
		switch (Random::Generate(9)) {
		case 0:
		case 1:
		case 2:
		case 3: graphic = '.'; break;
		case 4:
		case 5:
		case 6:
		case 7: graphic = ','; break;
		case 8: graphic = ':'; break;
		case 9: graphic = '\''; break;
		}
	} else if (type == TILEDITCH || type == TILERIVERBED) {
		vis = true; walkable = true; buildable = true; low = true;
		graphic = '_';
		originalForeColor = TCODColor(125,50,0);
		moveCost = Random::Generate(3, 5);
	} else if (type == TILEBOG) {
		vis = true; walkable = true; buildable = true; low = false;
		switch (Random::Generate(9)) {
		case 0:
		case 1:
		case 2:
		case 3: graphic = '~'; break;
		case 4:
		case 5:
		case 6:
		case 7: graphic = ','; break;
		case 8: graphic = ':'; break;
		case 9: graphic = '\''; break;
		}
		originalForeColor = TCODColor(Random::Generate(184), 127, 70);
		backColor = TCODColor(60,30,20);
		moveCost = Random::Generate(6, 10);
	} else if (type == TILEROCK) {
		vis = true; walkable = true; buildable = true; low = false;
		graphic = (Random::GenerateBool() ? ',' : '.');
		originalForeColor = TCODColor(Random::Generate(182, 182 + 19), Random::Generate(182, 182 + 19), Random::Generate(182, 182 + 19));
		backColor = TCODColor(0, 0, 0);
	} else if (type == TILEMUD) {
		vis = true; walkable = true; buildable = true; low = false;
		switch (rand() % 2) {
		case 0: graphic = '~'; break;
		case 1: graphic = '#'; break;
		}
		originalForeColor = TCODColor(120 + rand() % 60, 80 + rand() % 50, 0);
		backColor = TCODColor(0, 0, 0);
		moveCost = 5;
	} else { vis = false; walkable = false; buildable = false; }
	foreColor = originalForeColor;
}

bool Tile::BlocksLight() const { return !vis; }
void Tile::BlocksLight(bool value) { vis = !value; }

bool Tile::IsWalkable() const {
	return walkable;
}
void Tile::SetWalkable(bool value) {
	std::queue<int> bumpQueue;
	walkable = value;
	if (value == false) {
		//We temporarily store the uids elsewhere so that we can safely
		//call them. Iterating through a set while modifying it destructively isn't safe
		for (std::set<int>::iterator npcIter = npcList.begin(); npcIter != npcList.end(); ++npcIter) {
			bumpQueue.push(*npcIter);
		}
		for (std::set<int>::iterator itemIter = itemList.begin(); itemIter != itemList.end(); ++itemIter) {
			bumpQueue.push(*itemIter);
		}
		while (!bumpQueue.empty()) { Game::Inst()->BumpEntity(bumpQueue.front()); bumpQueue.pop(); }
	}
}

bool Tile::BlocksWater() const { return blocksWater; }
void Tile::BlocksWater(bool value) { blocksWater = value; }

int Tile::MoveCost(void* ptr) const {
	if (!static_cast<NPC*>(ptr)->HasHands()) {
		if (construction >= 0) {
			if (boost::shared_ptr<Construction> cons = Game::Inst()->GetConstruction(construction).lock()) {
				if (cons->HasTag(DOOR)) return MoveCost()+50;
			}
		}
	}
	int cost = MoveCost();
	if (cost == 0 && construction >= 0 && static_cast<NPC*>(ptr)->IsTunneler()) return 50;
	return cost;
}

int Tile::MoveCost() const {
	if (!IsWalkable()) return 0;
	int cost = moveCost;
	if (construction >= 0) cost += 2;

	//If a construction exists here, and it's a bridge, then movecost = 1 (disregards mud/ditch/etc)
	if (construction > 0 && Game::Inst()->GetConstruction(construction).lock() && 
		Game::Inst()->GetConstruction(construction).lock()->HasTag(BRIDGE)) {
			cost = 1;
	} else if (water) { //Otherwise take water depth into account
		cost += std::min(20, water->Depth());
	}

	return cost;
}
void Tile::SetMoveCost(int value) { moveCost = value; }

void Tile::Buildable(bool value) { buildable = value; }
bool Tile::Buildable() const { return buildable; }

void Tile::MoveFrom(int uid) {
	if (npcList.find(uid) == npcList.end()) {
#ifdef DEBUG
		std::cout<<"\nNPC "<<uid<<" moved off of empty list";
		std::cout<<"\nlist.size(): "<<npcList.size();
		std::cout<<"\nNPC: "<<Game::Inst()->npcList[uid]->Position().X()<<","<<Game::Inst()->npcList[uid]->Position().Y()<<'\n';
#endif
		return;
	}
	npcList.erase(npcList.find(uid));
}

void Tile::MoveTo(int uid) {
	npcList.insert(uid);		
}

void Tile::SetConstruction(int uid) { construction = uid; }
int Tile::GetConstruction() const { return construction; }

boost::weak_ptr<WaterNode> Tile::GetWater() const {return boost::weak_ptr<WaterNode>(water);}
void Tile::SetWater(boost::shared_ptr<WaterNode> value) {water = value;}

bool Tile::Low() const {return low;}
void Tile::Low(bool value) {low = value;}

int Tile::Graphic() const { return graphic; }
TCODColor Tile::ForeColor() const { 
	return foreColor;
}
TCODColor Tile::BackColor() const {
	if (!blood && !marked) return backColor;
	TCODColor result = backColor;
	if (blood)
		result.r = std::min(255, backColor.r + blood->Depth());
	if (marked)
		result = result + TCODColor::darkGrey;
	return result; 
}

void Tile::NatureObject(int val) { natureObject = val; }
int Tile::NatureObject() const { return natureObject; }

boost::weak_ptr<FilthNode> Tile::GetFilth() const {return boost::weak_ptr<FilthNode>(filth);}
void Tile::SetFilth(boost::shared_ptr<FilthNode> value) {filth = value;}

boost::weak_ptr<BloodNode> Tile::GetBlood() const {return boost::weak_ptr<BloodNode>(blood);}
void Tile::SetBlood(boost::shared_ptr<BloodNode> value) {blood = value;}

void Tile::Mark() { marked = true; }
void Tile::Unmark() { marked = false; }

void Tile::WalkOver() {
	//Ground under a construction wont turn to mud
	if (walkedOver < 120 || construction < 0) ++walkedOver;
	if (type == TILEGRASS) {
		foreColor = originalForeColor + TCODColor(std::min(255, walkedOver), 0, 0) - TCODColor(0, std::min(255,corruption), 0);
		if (walkedOver > 100 && graphic != '.' && graphic != ',') graphic = Random::GenerateBool() ? '.' : ',';
		if (walkedOver > 300 && rand() % 100 == 0) SetType(TILEMUD);
	}
}

void Tile::Corrupt(int magnitude) {
	corruption += magnitude;
	if (corruption < 0) corruption = 0;
	if (type == TILEGRASS) {
		foreColor = originalForeColor + TCODColor(std::min(255, walkedOver), 0, 0) - TCODColor(0, std::min(255,corruption), 0);;
	}
}

TileType Tile::StringToTileType(std::string string) {
	if (boost::iequals(string, "grass")) {
		return TILEGRASS;
	} else if (boost::iequals(string, "river")) {
		return TILERIVERBED;
	} else if (boost::iequals(string, "ditch")) {
		return TILEDITCH;
	} else if (boost::iequals(string, "rock")) {
		return TILEROCK;
	} else if (boost::iequals(string, "mud")) {
		return TILEMUD;
	} else if (boost::iequals(string, "bog")) {
		return TILEBOG;
	}
	return TILENONE;
}
