/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * $URL$
 * $Id$
 *
 */

#include "saga/saga.h"

#include "saga/actor.h"
#include "saga/animation.h"
#include "saga/console.h"
#include "saga/events.h"
#include "saga/isomap.h"
#include "saga/objectmap.h"
#include "saga/resource.h"
#include "saga/script.h"
#include "saga/sndres.h"
#include "saga/sound.h"
#include "saga/scene.h"

#include "common/config-manager.h"

namespace Saga {

ActorData::ActorData() {
	memset(this, 0, sizeof(*this));
}

ActorData::~ActorData() {
	if (!_shareFrames)
		free(_frames);
	free(_tileDirections);
	free(_walkStepsPoints);
	freeSpriteList();
}
void ActorData::saveState(Common::OutSaveFile *out) {
	int i = 0;
	CommonObjectData::saveState(out);
	out->writeUint16LE(_actorFlags);
	out->writeSint32LE(_currentAction);
	out->writeSint32LE(_facingDirection);
	out->writeSint32LE(_actionDirection);
	out->writeSint32LE(_actionCycle);
	out->writeUint16LE(_targetObject);

	out->writeSint32LE(_cycleFrameSequence);
	out->writeByte(_cycleDelay);
	out->writeByte(_cycleTimeCount);
	out->writeByte(_cycleFlags);
	out->writeSint16LE(_fallVelocity);
	out->writeSint16LE(_fallAcceleration);
	out->writeSint16LE(_fallPosition);
	out->writeByte(_dragonBaseFrame);
	out->writeByte(_dragonStepCycle);
	out->writeByte(_dragonMoveType);
	out->writeSint32LE(_frameNumber);

	out->writeSint32LE(_tileDirectionsAlloced);
	for (i = 0; i < _tileDirectionsAlloced; i++) {
		out->writeByte(_tileDirections[i]);
	}

	out->writeSint32LE(_walkStepsAlloced);
	for (i = 0; i < _walkStepsAlloced; i++) {
		out->writeSint16LE(_walkStepsPoints[i].x);
		out->writeSint16LE(_walkStepsPoints[i].y);
	}

	out->writeSint32LE(_walkStepsCount);
	out->writeSint32LE(_walkStepIndex);
	_finalTarget.saveState(out);
	_partialTarget.saveState(out);
	out->writeSint32LE(_walkFrameSequence);
}

void ActorData::loadState(uint32 version, Common::InSaveFile *in) {
	int i = 0;
	CommonObjectData::loadState(in);
	_actorFlags = in->readUint16LE();
	_currentAction = in->readSint32LE();
	_facingDirection = in->readSint32LE();
	_actionDirection = in->readSint32LE();
	_actionCycle = in->readSint32LE();
	_targetObject = in->readUint16LE();

	_lastZone = NULL;
	_cycleFrameSequence = in->readSint32LE();
	_cycleDelay = in->readByte();
	_cycleTimeCount = in->readByte();
	_cycleFlags = in->readByte();
	if (version > 1) {
		_fallVelocity = in->readSint16LE();
		_fallAcceleration = in->readSint16LE();
		_fallPosition = in->readSint16LE();
	} else {
		_fallVelocity = _fallAcceleration = _fallPosition = 0;
	}
	if (version > 2) {
		_dragonBaseFrame = in->readByte();
		_dragonStepCycle = in->readByte();
		_dragonMoveType = in->readByte();
	} else {
		_dragonBaseFrame = _dragonStepCycle = _dragonMoveType = 0;
	}

	_frameNumber = in->readSint32LE();


	setTileDirectionsSize(in->readSint32LE(), true);
	for (i = 0; i < _tileDirectionsAlloced; i++) {
		_tileDirections[i] = in->readByte();
	}

	setWalkStepsPointsSize(in->readSint32LE(), true);
	for (i = 0; i < _walkStepsAlloced; i++) {
		_walkStepsPoints[i].x = in->readSint16LE();
		_walkStepsPoints[i].y = in->readSint16LE();
	}

	_walkStepsCount = in->readSint32LE();
	_walkStepIndex = in->readSint32LE();
	_finalTarget.loadState(in);
	_partialTarget.loadState(in);
	_walkFrameSequence = in->readSint32LE();
}

void ActorData::setTileDirectionsSize(int size, bool forceRealloc) {
	if ((size <= _tileDirectionsAlloced) && !forceRealloc) {
		return;
	}
	_tileDirectionsAlloced = size;
	_tileDirections = (byte*)realloc(_tileDirections, _tileDirectionsAlloced * sizeof(*_tileDirections));
}

void ActorData::cycleWrap(int cycleLimit) {
	if (_actionCycle >= cycleLimit)
		_actionCycle = 0;
}

void ActorData::setWalkStepsPointsSize(int size, bool forceRealloc) {
	if ((size <= _walkStepsAlloced) && !forceRealloc) {
		return;
	}
	_walkStepsAlloced = size;
	_walkStepsPoints = (Point*)realloc(_walkStepsPoints, _walkStepsAlloced * sizeof(*_walkStepsPoints));
}

void ActorData::addWalkStepPoint(const Point &point) {
	setWalkStepsPointsSize(_walkStepsCount + 1, false);
	_walkStepsPoints[_walkStepsCount++] = point;
}

void ActorData::freeSpriteList() {
	_spriteList.freeMem();
}



static int commonObjectCompare(const CommonObjectDataPointer& obj1, const CommonObjectDataPointer& obj2) {
	int p1 = obj1->_location.y - obj1->_location.z;
	int p2 = obj2->_location.y - obj2->_location.z;
	if (p1 == p2)
		return 0;
	if (p1 < p2)
		return -1;
	return 1;
}

#ifdef ENABLE_IHNM
static int commonObjectCompareIHNM(const CommonObjectDataPointer& obj1, const CommonObjectDataPointer& obj2) {
	int p1 = obj1->_location.y;
	int p2 = obj2->_location.y;
	if (p1 == p2)
		return 0;
	if (p1 < p2)
		return -1;
	return 1;
}
#endif

static int tileCommonObjectCompare(const CommonObjectDataPointer& obj1, const CommonObjectDataPointer& obj2) {
	int p1 = -obj1->_location.u() - obj1->_location.v() - obj1->_location.z;
	int p2 = -obj2->_location.u() - obj2->_location.v() - obj2->_location.z;
	//TODO:  for kObjNotFlat obj Height*3 of sprite should be added to p1 and p2
	//if (validObjId(obj1->id)) {

	if (p1 == p2)
		return 0;
	if (p1 < p2)
		return -1;
	return 1;
}

Actor::Actor(SagaEngine *vm) : _vm(vm) {
	int i;
	byte *stringsPointer;
	size_t stringsLength;
	ActorData *actor;
	ObjectData *obj;
	debug(9, "Actor::Actor()");
	_handleActionDiv = 15;

	_actors = NULL;
	_actorsCount = 0;

	_objs = NULL;
	_objsCount = 0;

#ifdef ACTOR_DEBUG
	_debugPointsCount = 0;
#endif

	_protagStates = NULL;
	_protagStatesCount = 0;

	_pathList = NULL;
	_pathListAlloced = 0;
	_pathListIndex = -1;

	_centerActor = _protagonist = NULL;
	_protagState = 0;
	_lastTickMsec = 0;

	_yCellCount = _vm->_scene->getHeight();
	_xCellCount = _vm->getDisplayInfo().width;

	_pathCell = (int8 *)malloc(_yCellCount * _xCellCount * sizeof(*_pathCell));

	_pathRect.left = 0;
	_pathRect.right = _vm->getDisplayInfo().width;
	_pathRect.top = _vm->getDisplayInfo().pathStartY;
	_pathRect.bottom = _vm->_scene->getHeight();

	// Get actor resource file context
	_actorContext = _vm->_resource->getContext(GAME_RESOURCEFILE);
	if (_actorContext == NULL) {
		error("Actor::Actor() resource context not found");
	}

	// Load ITE actor strings. (IHNM actor strings are loaded by
	// loadGlobalResources() instead.)

	if (_vm->getGameId() == GID_ITE) {

		_vm->_resource->loadResource(_actorContext, _vm->getResourceDescription()->actorsStringsResourceId, stringsPointer, stringsLength);

		_vm->loadStrings(_actorsStrings, stringsPointer, stringsLength);
		free(stringsPointer);
	}

	if (_vm->getGameId() == GID_ITE) {
		_actorsCount = ITE_ACTORCOUNT;
		_actors = (ActorData **)malloc(_actorsCount * sizeof(*_actors));
		for (i = 0; i < _actorsCount; i++) {
			actor = _actors[i] = new ActorData();
			actor->_id = actorIndexToId(i);
			actor->_index = i;
			debug(9, "init actor id=%d index=%d", actor->_id, actor->_index);
			actor->_nameIndex = ITE_ActorTable[i].nameIndex;
			actor->_scriptEntrypointNumber = ITE_ActorTable[i].scriptEntrypointNumber;
			actor->_spriteListResourceId = ITE_ActorTable[i].spriteListResourceId;
			actor->_frameListResourceId = ITE_ActorTable[i].frameListResourceId;
			actor->_speechColor = ITE_ActorTable[i].speechColor;
			actor->_sceneNumber = ITE_ActorTable[i].sceneIndex;
			actor->_flags = ITE_ActorTable[i].flags;
			actor->_currentAction = ITE_ActorTable[i].currentAction;
			actor->_facingDirection = ITE_ActorTable[i].facingDirection;
			actor->_actionDirection = ITE_ActorTable[i].actionDirection;

			actor->_location.x = ITE_ActorTable[i].x;
			actor->_location.y = ITE_ActorTable[i].y;
			actor->_location.z = ITE_ActorTable[i].z;

			actor->_disabled = !loadActorResources(actor);
			if (actor->_disabled) {
				warning("Disabling actor Id=%d index=%d", actor->_id, actor->_index);
			}
		}
		_objsCount = ITE_OBJECTCOUNT;
		_objs = (ObjectData **)malloc(_objsCount * sizeof(*_objs));
		for (i = 0; i < _objsCount; i++) {
			obj = _objs[i] = new ObjectData();
			obj->_id = objIndexToId(i);
			obj->_index = i;
			debug(9, "init obj id=%d index=%d", obj->_id, obj->_index);
			obj->_nameIndex = ITE_ObjectTable[i].nameIndex;
			obj->_scriptEntrypointNumber = ITE_ObjectTable[i].scriptEntrypointNumber;
			obj->_spriteListResourceId = ITE_ObjectTable[i].spriteListResourceId;
			obj->_sceneNumber = ITE_ObjectTable[i].sceneIndex;
			obj->_interactBits = ITE_ObjectTable[i].interactBits;

			obj->_location.x = ITE_ObjectTable[i].x;
			obj->_location.y = ITE_ObjectTable[i].y;
			obj->_location.z = ITE_ObjectTable[i].z;
		}
	}

	_dragonHunt = true;
}

Actor::~Actor() {
	debug(9, "Actor::~Actor()");

	free(_pathList);
	free(_pathCell);
	_actorsStrings.freeMem();
	//release resources
	freeProtagStates();
	freeActorList();
	freeObjList();
}

void Actor::freeProtagStates() {
	int i;
	for (i = 0; i < _protagStatesCount; i++) {
		free(_protagStates[i]._frames);
	}
	free(_protagStates);
	_protagStates = NULL;
	_protagStatesCount = 0;
}

void Actor::loadFrameList(int frameListResourceId, ActorFrameSequence *&framesPointer, int &framesCount) {
	byte *resourcePointer;
	size_t resourceLength;

	debug(9, "Loading frame resource id %d", frameListResourceId);
	_vm->_resource->loadResource(_actorContext, frameListResourceId, resourcePointer, resourceLength);

	framesCount = resourceLength / 16;
	debug(9, "Frame resource contains %d frames (res length is %d)", framesCount, (int)resourceLength);

	framesPointer = (ActorFrameSequence *)malloc(sizeof(ActorFrameSequence) * framesCount);
	if (framesPointer == NULL && framesCount != 0) {
		memoryError("Actor::loadFrameList");
	}

	MemoryReadStreamEndian readS(resourcePointer, resourceLength, _actorContext->isBigEndian);

	for (int i = 0; i < framesCount; i++) {
		debug(9, "frameType %d", i);
		for (int orient = 0; orient < ACTOR_DIRECTIONS_COUNT; orient++) {
			// Load all four orientations
			framesPointer[i].directions[orient].frameIndex = readS.readUint16();
			if (_vm->getGameId() == GID_ITE) {
				framesPointer[i].directions[orient].frameCount = readS.readSint16();
			} else {
				framesPointer[i].directions[orient].frameCount = readS.readByte();
				readS.readByte();
			}
			if (framesPointer[i].directions[orient].frameCount < 0)
				warning("frameCount < 0 (%d)", framesPointer[i].directions[orient].frameCount);
			debug(9, "frameIndex %d frameCount %d", framesPointer[i].directions[orient].frameIndex, framesPointer[i].directions[orient].frameCount);
		}
	}

	free(resourcePointer);
}

bool Actor::loadActorResources(ActorData *actor) {
	bool gotSomething = false;

	if (actor->_frameListResourceId) {
		loadFrameList(actor->_frameListResourceId, actor->_frames, actor->_framesCount);

		actor->_shareFrames = false;

		gotSomething = true;
	} else {
		// It's normal for some actors to have no frames
		//warning("Frame List ID = 0 for actor index %d", actor->_index);

		//if (_vm->getGameId() == GID_ITE)
		return true;
	}

	if (actor->_spriteListResourceId) {
		gotSomething = true;
	} else {
		warning("Sprite List ID = 0 for actor index %d", actor->_index);
	}

	return gotSomething;
}

void Actor::freeActorList() {
	int i;
	ActorData *actor;
	for (i = 0; i < _actorsCount; i++) {
		actor = _actors[i];
		delete actor;
	}
	free(_actors);
	_actors = NULL;
	_actorsCount = 0;
}

void Actor::loadActorSpriteList(ActorData *actor) {
	int lastFrame = 0;
	int resourceId = actor->_spriteListResourceId;

	for (int i = 0; i < actor->_framesCount; i++) {
		for (int orient = 0; orient < ACTOR_DIRECTIONS_COUNT; orient++) {
			if (actor->_frames[i].directions[orient].frameIndex > lastFrame) {
				lastFrame = actor->_frames[i].directions[orient].frameIndex;
			}
		}
	}

	debug(9, "Loading actor sprite resource id %d", resourceId);

	_vm->_sprite->loadList(resourceId, actor->_spriteList);

	if (_vm->getGameId() == GID_ITE) {
		if (actor->_flags & kExtended) {
			while ((lastFrame >= actor->_spriteList.spriteCount)) {
				resourceId++;
				debug(9, "Appending to actor sprite list %d", resourceId);
				_vm->_sprite->loadList(resourceId, actor->_spriteList);
			}
		}
	}
}

void Actor::loadActorList(int protagonistIdx, int actorCount, int actorsResourceID, int protagStatesCount, int protagStatesResourceID) {
	int i, j;
	ActorData *actor;
	byte* actorListData;
	size_t actorListLength;
	byte walk[128];
	byte acv[6];
	int movementSpeed;
	int walkStepIndex;
	int walkStepCount;
	int stateResourceId;

	freeActorList();

	_vm->_resource->loadResource(_actorContext, actorsResourceID, actorListData, actorListLength);

	_actorsCount = actorCount;

	if (actorListLength != (uint)_actorsCount * ACTOR_INHM_SIZE) {
		error("Actor::loadActorList wrong actorlist length");
	}

	MemoryReadStream actorS(actorListData, actorListLength);

	_actors = (ActorData **)malloc(_actorsCount * sizeof(*_actors));
	for (i = 0; i < _actorsCount; i++) {
		actor = _actors[i] = new ActorData();
		actor->_id = objectIndexToId(kGameObjectActor, i); //actorIndexToId(i);
		actor->_index = i;
		debug(4, "init actor id=0x%x index=%d", actor->_id, actor->_index);
		actorS.readUint32LE(); //next displayed
		actorS.readByte(); //type
		actor->_flags = actorS.readByte();
		actor->_nameIndex = actorS.readUint16LE();
		actor->_sceneNumber = actorS.readUint32LE();
		actor->_location.fromStream(actorS);
		actor->_screenPosition.x = actorS.readUint16LE();
		actor->_screenPosition.y = actorS.readUint16LE();
		actor->_screenScale = actorS.readUint16LE();
		actor->_screenDepth = actorS.readUint16LE();
		actor->_spriteListResourceId = actorS.readUint32LE();
		actor->_frameListResourceId = actorS.readUint32LE();
		debug(4, "%d: %d, %d [%d]", i, actor->_spriteListResourceId, actor->_frameListResourceId, actor->_nameIndex);
		actor->_scriptEntrypointNumber = actorS.readUint32LE();
		actorS.readUint32LE(); // xSprite *dSpr;
		actorS.readUint16LE(); //LEFT
		actorS.readUint16LE(); //RIGHT
		actorS.readUint16LE(); //TOP
		actorS.readUint16LE(); //BOTTOM
		actor->_speechColor = actorS.readByte();
		actor->_currentAction = actorS.readByte();
		actor->_facingDirection = actorS.readByte();
		actor->_actionDirection = actorS.readByte();
		actor->_actionCycle = actorS.readUint16LE();
		actor->_frameNumber = actorS.readUint16LE();
		actor->_finalTarget.fromStream(actorS);
		actor->_partialTarget.fromStream(actorS);
		movementSpeed = actorS.readUint16LE(); //movement speed
		if (movementSpeed) {
			error("Actor::loadActorList movementSpeed != 0");
		}
		actorS.read(walk, 128);
		for (j = 0; j < 128; j++) {
			if (walk[j]) {
				error("Actor::loadActorList walk[128] != 0");
			}
		}
		//actorS.seek(128, SEEK_CUR);
		walkStepCount = actorS.readByte();//walkStepCount
		if (walkStepCount) {
			error("Actor::loadActorList walkStepCount != 0");
		}
		walkStepIndex = actorS.readByte();//walkStepIndex
		if (walkStepIndex) {
			error("Actor::loadActorList walkStepIndex != 0");
		}
		//no need to check pointers
		actorS.readUint32LE(); //sprites
		actorS.readUint32LE(); //frames
		actorS.readUint32LE(); //last zone
		actor->_targetObject = actorS.readUint16LE();
		actor->_actorFlags = actorS.readUint16LE();
		//no need to check pointers
		actorS.readUint32LE(); //next in scene
		actorS.read(acv, 6);
		for (j = 0; j < 6; j++) {
			if (acv[j]) {
				error("Actor::loadActorList acv[%d] != 0", j);
			}
		}
//		actorS.seek(6, SEEK_CUR); //action vars
	}
	free(actorListData);

	_actors[protagonistIdx]->_flags |= kProtagonist | kExtended;

	for (i = 0; i < _actorsCount; i++) {
		actor = _actors[i];
		//if (actor->_flags & kProtagonist) {
			loadActorResources(actor);
			//break;
		//}
	}

	_centerActor = _protagonist = _actors[protagonistIdx];
	_protagState = 0;

	if (protagStatesResourceID) {
		if (!_protagonist->_shareFrames)
			free(_protagonist->_frames);
		freeProtagStates();

		_protagStates = (ProtagStateData *)malloc(sizeof(ProtagStateData) * protagStatesCount);

		byte *idsResourcePointer;
		size_t idsResourceLength;

		_vm->_resource->loadResource(_actorContext, protagStatesResourceID,
									 idsResourcePointer, idsResourceLength);

		if (idsResourceLength < (size_t)protagStatesCount * 4) {
			error("Wrong protagonist states resource");
		}

		MemoryReadStream statesIds(idsResourcePointer, idsResourceLength);

		for (i = 0; i < protagStatesCount; i++) {
			stateResourceId = statesIds.readUint32LE();

			loadFrameList(stateResourceId, _protagStates[i]._frames, _protagStates[i]._framesCount);
		}
		free(idsResourcePointer);

		_protagonist->_frames = _protagStates[_protagState]._frames;
		_protagonist->_framesCount = _protagStates[_protagState]._framesCount;
		_protagonist->_shareFrames = true;
	}

	_protagStatesCount = protagStatesCount;
}

void Actor::freeObjList() {
	int i;
	ObjectData *object;
	for (i = 0; i < _objsCount; i++) {
		object = _objs[i];
		delete object;
	}
	free(_objs);
	_objs = NULL;
	_objsCount = 0;
}

void Actor::loadObjList(int objectCount, int objectsResourceID) {
	int i;
	int frameListResourceId;
	ObjectData *object;
	byte* objectListData;
	size_t objectListLength;
	freeObjList();

	_vm->_resource->loadResource(_actorContext, objectsResourceID, objectListData, objectListLength);

	_objsCount = objectCount;

	MemoryReadStream objectS(objectListData, objectListLength);

	_objs = (ObjectData **)malloc(_objsCount * sizeof(*_objs));
	for (i = 0; i < _objsCount; i++) {
		object = _objs[i] = new ObjectData();
		object->_id = objectIndexToId(kGameObjectObject, i);
		object->_index = i;
		debug(9, "init object id=%d index=%d", object->_id, object->_index);
		objectS.readUint32LE(); //next displayed
		objectS.readByte(); //type
		object->_flags = objectS.readByte();
		object->_nameIndex = objectS.readUint16LE();
		object->_sceneNumber = objectS.readUint32LE();
		object->_location.fromStream(objectS);
		object->_screenPosition.x = objectS.readUint16LE();
		object->_screenPosition.y = objectS.readUint16LE();
		object->_screenScale = objectS.readUint16LE();
		object->_screenDepth = objectS.readUint16LE();
		object->_spriteListResourceId = objectS.readUint32LE();
		frameListResourceId = objectS.readUint32LE(); // object->_frameListResourceId
		if (frameListResourceId) {
			error("Actor::loadObjList frameListResourceId != 0");
		}
		object->_scriptEntrypointNumber = objectS.readUint32LE();
		objectS.readUint32LE(); // xSprite *dSpr;
		objectS.readUint16LE(); //LEFT
		objectS.readUint16LE(); //RIGHT
		objectS.readUint16LE(); //TOP
		objectS.readUint16LE(); //BOTTOM
		object->_interactBits = objectS.readUint16LE();
	}
	free(objectListData);
}

void Actor::takeExit(uint16 actorId, const HitZone *hitZone) {
	ActorData *actor;
	actor = getActor(actorId);
	actor->_lastZone = NULL;

	_vm->_scene->changeScene(hitZone->getSceneNumber(), hitZone->getActorsEntrance(), kTransitionNoFade);
	if (_vm->_interface->getMode() != kPanelSceneSubstitute) {
		_vm->_script->setNoPendingVerb();
	}
}

void Actor::stepZoneAction(ActorData *actor, const HitZone *hitZone, bool exit, bool stopped) {
	Event event;

	if (actor != _protagonist) {
		return;
	}
	if (((hitZone->getFlags() & kHitZoneTerminus) && !stopped) || (!(hitZone->getFlags() & kHitZoneTerminus) && stopped)) {
		return;
	}

	if (!exit) {
		if (hitZone->getFlags() & kHitZoneAutoWalk) {
			actor->_currentAction = kActionWalkDir;
			actor->_actionDirection = actor->_facingDirection = hitZone->getDirection();
			actor->_walkFrameSequence = getFrameType(kFrameWalk);
			return;
		}
	} else if (!(hitZone->getFlags() & kHitZoneAutoWalk)) {
		return;
	}
	if (hitZone->getFlags() & kHitZoneExit) {
		takeExit(actor->_id, hitZone);
	} else if (hitZone->getScriptNumber() > 0) {
		event.type = kEvTOneshot;
		event.code = kScriptEvent;
		event.op = kEventExecNonBlocking;
		event.time = 0;
		event.param = _vm->_scene->getScriptModuleNumber(); // module number
		event.param2 = hitZone->getScriptNumber();			// script entry point number
		event.param3 = _vm->_script->getVerbType(kVerbEnter);		// Action
		event.param4 = ID_NOTHING;		// Object
		event.param5 = ID_NOTHING;		// With Object
		event.param6 = ID_PROTAG;		// Actor

		_vm->_events->queue(&event);
	}
}

ObjectData *Actor::getObj(uint16 objId) {
	ObjectData *obj;

	if (!validObjId(objId))
		error("Actor::getObj Wrong objId 0x%X", objId);

	obj = _objs[objIdToIndex(objId)];

	if (obj->_disabled)
		error("Actor::getObj disabled objId 0x%X", objId);

	return obj;
}

ActorData *Actor::getActor(uint16 actorId) {
	ActorData *actor;

	if (!validActorId(actorId)) {
		warning("Actor::getActor Wrong actorId 0x%X", actorId);
		assert(0);
	}

	if (actorId == ID_PROTAG) {
		if (_protagonist == NULL) {
			error("_protagonist == NULL");
		}
		return _protagonist;
	}

	actor = _actors[actorIdToIndex(actorId)];

	if (actor->_disabled)
		error("Actor::getActor disabled actorId 0x%X", actorId);

	return actor;
}

void Actor::setProtagState(int state) {
	_protagState = state;

#ifdef ENABLE_IHNM
	if (_vm->getGameId() == GID_IHNM) {
		if (!_protagonist->_shareFrames)
			free(_protagonist->_frames);

		_protagonist->_frames = _protagStates[state]._frames;
		_protagonist->_framesCount = _protagStates[state]._framesCount;
		_protagonist->_shareFrames = true;
	}
#endif

}

int Actor::getFrameType(ActorFrameTypes frameType) {

	if (_vm->getGameId() == GID_ITE) {
		switch (frameType) {
		case kFrameStand:
			return kFrameITEStand;
		case kFrameWalk:
			return kFrameITEWalk;
		case kFrameSpeak:
			return kFrameITESpeak;
		case kFrameGive:
			return kFrameITEGive;
		case kFrameGesture:
			return kFrameITEGesture;
		case kFrameWait:
			return kFrameITEWait;
		case kFramePickUp:
			return kFrameITEPickUp;
		case kFrameLook:
			return kFrameITELook;
		}
#ifdef ENABLE_IHNM
	} else if (_vm->getGameId() == GID_IHNM) {
		switch (frameType) {
		case kFrameStand:
			return kFrameIHNMStand;
		case kFrameWalk:
			return kFrameIHNMWalk;
		case kFrameSpeak:
			return kFrameIHNMSpeak;
		case kFrameGesture:
			return kFrameIHNMGesture;
		case kFrameWait:
			return kFrameIHNMWait;
		case kFrameGive:
		case kFramePickUp:
		case kFrameLook:
			error("Actor::getFrameType() unknown frame type %d", frameType);
			return kFrameIHNMStand;
		}
#endif
	}
	error("Actor::getFrameType() unknown frame type %d", frameType);
}

ActorFrameRange *Actor::getActorFrameRange(uint16 actorId, int frameType) {
	ActorData *actor;
	int fourDirection;
	static ActorFrameRange def = {0, 0};

	actor = getActor(actorId);
	if (actor->_disabled)
		error("Actor::getActorFrameRange Wrong actorId 0x%X", actorId);

	if ((actor->_facingDirection < kDirUp) || (actor->_facingDirection > kDirUpLeft))
		error("Actor::getActorFrameRange Wrong direction 0x%X actorId 0x%X", actor->_facingDirection, actorId);

	if (_vm->getGameId() == GID_ITE) {
		if (frameType >= actor->_framesCount) {
			warning("Actor::getActorFrameRange Wrong frameType 0x%X (%d) actorId 0x%X", frameType, actor->_framesCount, actorId);
			return &def;
		}


		fourDirection = actorDirectionsLUT[actor->_facingDirection];
		return &actor->_frames[frameType].directions[fourDirection];
	}

#ifdef ENABLE_IHNM
	if (_vm->getGameId() == GID_IHNM) {
		// It is normal for some actors to have no frames for a given frameType
		// These are mainly actors with no frames at all (e.g. narrators or immovable actors)
		// Examples are AM and the boy when he is talking to Benny via the computer screen.
		// Both of them are invisible and immovable
		// There is no point to keep throwing warnings about this, the original checks for
		// a valid framecount too
		if (actor->_framesCount == 0) {
			return &def;
		}
		frameType = CLIP(frameType, 0, actor->_framesCount - 1);
		fourDirection = actorDirectionsLUT[actor->_facingDirection];
		return &actor->_frames[frameType].directions[fourDirection];
	}
#endif

	return NULL;
}

void Actor::handleSpeech(int msec) {
	int stringLength;
	int sampleLength;
	bool removeFirst;
	int i;
	ActorData *actor;
	int width, height, height2;

	if (_activeSpeech.playing) {
		_activeSpeech.playingTime -= msec;
		stringLength = strlen(_activeSpeech.strings[0]);

		removeFirst = false;
		if (_activeSpeech.playingTime <= 0) {
			if (_activeSpeech.speechFlags & kSpeakSlow) {
				_activeSpeech.slowModeCharIndex++;
				if (_activeSpeech.slowModeCharIndex >= stringLength)
					removeFirst = true;
			} else {
				removeFirst = true;
			}
			_activeSpeech.playing = false;
			if (_activeSpeech.speechFlags & kSpeakForceText)
				_activeSpeech.speechFlags = 0;
			if (_activeSpeech.actorIds[0] != 0) {
				actor = getActor(_activeSpeech.actorIds[0]);
				if (!(_activeSpeech.speechFlags & kSpeakNoAnimate)) {
					actor->_currentAction = kActionWait;
				}
			}
		}

		if (removeFirst) {
			for (i = 1; i < _activeSpeech.stringsCount; i++) {
				_activeSpeech.strings[i - 1] = _activeSpeech.strings[i];
			}
			_activeSpeech.stringsCount--;
		}

		if (_vm->_script->_skipSpeeches) {
			_activeSpeech.stringsCount = 0;
			_vm->_script->wakeUpThreads(kWaitTypeSpeech);
			return;
		}

		if (_activeSpeech.stringsCount == 0) {
			_vm->_script->wakeUpThreadsDelayed(kWaitTypeSpeech, _vm->ticksToMSec(kScriptTimeTicksPerSecond / 3));
		}

		return;
	}

	if (_vm->_script->_skipSpeeches) {
		_activeSpeech.stringsCount = 0;
		_vm->_script->wakeUpThreads(kWaitTypeSpeech);
	}

	if (_activeSpeech.stringsCount == 0) {
		return;
	}

	stringLength = strlen(_activeSpeech.strings[0]);

	if (_activeSpeech.speechFlags & kSpeakSlow) {
		if (_activeSpeech.slowModeCharIndex >= stringLength)
			error("Wrong string index");

		_activeSpeech.playingTime = 1000 / 8;

	} else {
		sampleLength = _vm->_sndRes->getVoiceLength(_activeSpeech.sampleResourceId);

		if (sampleLength < 0) {
			_activeSpeech.playingTime = stringLength * 1000 / 22;
			switch (_vm->_readingSpeed) {
			case 2:
				_activeSpeech.playingTime *= 2;
				break;
			case 1:
				_activeSpeech.playingTime *= 4;
				break;
			case 0:
				_activeSpeech.playingTime = 0x7fffff;
				break;
			}
		} else {
			_activeSpeech.playingTime = sampleLength;
		}
	}

	if (_activeSpeech.sampleResourceId != -1) {
		_vm->_sndRes->playVoice(_activeSpeech.sampleResourceId);
		_activeSpeech.sampleResourceId++;
	}

	if (_activeSpeech.actorIds[0] != 0) {
		actor = getActor(_activeSpeech.actorIds[0]);
		if (!(_activeSpeech.speechFlags & kSpeakNoAnimate)) {
			actor->_currentAction = kActionSpeak;
			actor->_actionCycle = _vm->_rnd.getRandomNumber(63);
		}
	}

	if (_activeSpeech.actorsCount == 1) {
		if (_speechBoxScript.width() > 0) {
			_activeSpeech.drawRect.left = _speechBoxScript.left;
			_activeSpeech.drawRect.right = _speechBoxScript.right;
			_activeSpeech.drawRect.top = _speechBoxScript.top;
			_activeSpeech.drawRect.bottom = _speechBoxScript.bottom;
		} else {
			width = _activeSpeech.speechBox.width();
			height = _vm->_font->getHeight(kKnownFontScript, _activeSpeech.strings[0], width - 2, _activeSpeech.getFontFlags(0)) + 1;

			if (_vm->getGameId() == GID_IHNM) {
				if (height > _vm->_scene->getHeight(true) / 2 && width < _vm->getDisplayInfo().width - 20) {
					width = _vm->getDisplayInfo().width - 20;
					height = _vm->_font->getHeight(kKnownFontScript, _activeSpeech.strings[0], width - 2, _activeSpeech.getFontFlags(0)) + 1;
				}
			} else if (_vm->getGameId() == GID_ITE) {
				if (height > 40 && width < _vm->getDisplayInfo().width - 100) {
					width = _vm->getDisplayInfo().width - 100;
					height = _vm->_font->getHeight(kKnownFontScript, _activeSpeech.strings[0], width - 2, _activeSpeech.getFontFlags(0)) + 1;
				}
			}

			_activeSpeech.speechBox.setWidth(width);

			if (_activeSpeech.actorIds[0] != 0) {
				actor = getActor(_activeSpeech.actorIds[0]);
				_activeSpeech.speechBox.setHeight(height);

				if (_activeSpeech.speechBox.right > _vm->getDisplayInfo().width - 10) {
					_activeSpeech.drawRect.left = _vm->getDisplayInfo().width - 10 - width;
				} else {
					_activeSpeech.drawRect.left = _activeSpeech.speechBox.left;
				}

				height2 = actor->_screenPosition.y - 50;
				if (height2 > _vm->_scene->getHeight(true))
					_activeSpeech.speechBox.top = _activeSpeech.drawRect.top = _vm->_scene->getHeight(true) - 1 - height - 10;
				else
					_activeSpeech.speechBox.top = _activeSpeech.drawRect.top = MAX(10, (height2 - height) / 2);
			} else {
				_activeSpeech.drawRect.left = _activeSpeech.speechBox.left;
				_activeSpeech.drawRect.top = _activeSpeech.speechBox.top + (_activeSpeech.speechBox.height() - height) / 2;
			}
			_activeSpeech.drawRect.setWidth(width);
			_activeSpeech.drawRect.setHeight(height);
		}
	}

	_activeSpeech.playing = true;
}

bool Actor::calcScreenPosition(CommonObjectData *commonObjectData) {
	int beginSlope, endSlope, middle;
	bool result;
	if (_vm->_scene->getFlags() & kSceneFlagISO) {
		_vm->_isoMap->tileCoordsToScreenPoint(commonObjectData->_location, commonObjectData->_screenPosition);
		commonObjectData->_screenScale = 256;
	} else {
		middle = _vm->_scene->getHeight() - commonObjectData->_location.y / ACTOR_LMULT;

		_vm->_scene->getSlopes(beginSlope, endSlope);

		commonObjectData->_screenDepth = (14 * middle) / endSlope + 1;

		if (middle <= beginSlope) {
			commonObjectData->_screenScale = 256;
#ifdef ENABLE_IHNM
		} else if (_vm->getGameId() == GID_IHNM && (objectTypeId(commonObjectData->_id) & kGameObjectObject)) {
			commonObjectData->_screenScale = 256;
		} else if (_vm->getGameId() == GID_IHNM && (commonObjectData->_flags & kNoScale)) {
			commonObjectData->_screenScale = 256;
#endif
		} else if (middle >= endSlope) {
			commonObjectData->_screenScale = 1;
		} else {
			middle -= beginSlope;
			endSlope -= beginSlope;
			commonObjectData->_screenScale = 256 - (middle * 256) / endSlope;
		}

		commonObjectData->_location.toScreenPointXYZ(commonObjectData->_screenPosition);
	}

	result = commonObjectData->_screenPosition.x > -64 &&
			commonObjectData->_screenPosition.x < _vm->getDisplayInfo().width + 64 &&
			commonObjectData->_screenPosition.y > -64 &&
			commonObjectData->_screenPosition.y < _vm->_scene->getHeight() + 64;

	return result;
}

uint16 Actor::hitTest(const Point &testPoint, bool skipProtagonist) {
	// We can only interact with objects or actors that are inside the
	// scene area. While this is usually the entire upper part of the
	// screen, it could also be an inset. Note that other kinds of hit
	// areas may be outside the inset, and that those are still perfectly
	// fine to interact with. For example, the door entrance at the glass
	// makers's house in ITE's ferret village.

	// Note that in IHNM, there are some items that overlap on other items
	// Since we're checking the draw list from the FIRST item drawn to the
	// LAST one, sometimes the object drawn first is incorrectly returned.
	// An example is the chalk on the magic circle in Ted's chapter, which
	// is drawn AFTER the circle, but HitTest incorrectly returns the circle
	// id in this case, even though the chalk was drawn after the circle.
	// Therefore, for IHNM, we iterate through the whole draw list and
	// return the last match found, not the first one.
	// Unfortunately, it is only possible to search items in the sorted draw
	// list from start to end, not reverse, so it's necessary to search
	// through the whole list to get the item drawn last

	uint16 result = ID_NOTHING;

	if (!_vm->_scene->getSceneClip().contains(testPoint))
		return ID_NOTHING;

	CommonObjectOrderList::iterator drawOrderIterator;
	CommonObjectDataPointer drawObject;
	int frameNumber = 0;
	SpriteList *spriteList = NULL;

	createDrawOrderList();

	for (drawOrderIterator = _drawOrderList.begin(); drawOrderIterator != _drawOrderList.end(); ++drawOrderIterator) {
		drawObject = *drawOrderIterator;
		if (skipProtagonist && (drawObject == _protagonist)) {
			continue;
		}
		if (!getSpriteParams(drawObject, frameNumber, spriteList)) {
			continue;
		}
		if (_vm->_sprite->hitTest(*spriteList, frameNumber, drawObject->_screenPosition, drawObject->_screenScale, testPoint)) {
			result = drawObject->_id;
			if (_vm->getGameId() == GID_ITE)
				return result;		// in ITE, return the first result found (read above)
		}
	}
	return result;					// in IHNM, return the last result found (read above)
}

void Actor::drawOrderListAdd(const CommonObjectDataPointer& element, CompareFunction compareFunction) {
	int res;

	for (CommonObjectOrderList::iterator i = _drawOrderList.begin(); i !=_drawOrderList.end(); ++i) {
		res = compareFunction(element, *i);
		if	(res < 0) {
			_drawOrderList.insert(i, element);
			return;
		}
	}
	_drawOrderList.push_back(element);
}

void Actor::createDrawOrderList() {
	int i;
	ActorData *actor;
	ObjectData *obj;
	CompareFunction compareFunction = 0;

	if (_vm->_scene->getFlags() & kSceneFlagISO) {
		compareFunction = &tileCommonObjectCompare;
	} else {
		if (_vm->getGameId() == GID_ITE)
			compareFunction = &commonObjectCompare;
#ifdef ENABLE_IHNM
		else if (_vm->getGameId() == GID_IHNM)
			compareFunction = &commonObjectCompareIHNM;
#endif
	}

	_drawOrderList.clear();
	for (i = 0; i < _actorsCount; i++) {
		actor = _actors[i];

		if (!actor->_inScene)
			continue;

		if (calcScreenPosition(actor)) {
			drawOrderListAdd(actor, compareFunction);
		}
	}

	for (i = 0; i < _objsCount; i++) {
		obj = _objs[i];
		if (obj->_disabled)
			continue;

		if (obj->_sceneNumber != _vm->_scene->currentSceneNumber())
			 continue;

		// WORKAROUND for a bug found in the original interpreter of IHNM
		// If an object's x or y value is negative, don't draw it
		// Scripts set negative values for an object's x and y when it shouldn't
		// be drawn anymore (i.e. when it's picked up or used)
		if (obj->_location.x < 0 || obj->_location.y < 0)
			continue;

		if (calcScreenPosition(obj)) {
			drawOrderListAdd(obj, compareFunction);
		}
	}
}

bool Actor::getSpriteParams(CommonObjectData *commonObjectData, int &frameNumber, SpriteList *&spriteList) {
	if (_vm->_scene->currentSceneResourceId() == ITE_SCENE_OVERMAP) {
		if (!(commonObjectData->_flags & kProtagonist)){
//			warning("not protagonist");
			return false;
		}
		frameNumber = 8;
		spriteList = &_vm->_sprite->_mainSprites;
	} else if (validActorId(commonObjectData->_id)) {
		ActorData *actor = (ActorData *)commonObjectData;
		spriteList = &(actor->_spriteList);
		frameNumber = actor->_frameNumber;
		if (spriteList->infoList == NULL)
			loadActorSpriteList(actor);

	} else if (validObjId(commonObjectData->_id)) {
		spriteList = &_vm->_sprite->_mainSprites;
		frameNumber = commonObjectData->_spriteListResourceId;
	}

	if (spriteList->spriteCount == 0) {
		return false;
	}

	if ((frameNumber < 0) || (spriteList->spriteCount <= frameNumber)) {
		debug(1, "Actor::getSpriteParams frameNumber invalid for %s id 0x%X (%d)",
				validObjId(commonObjectData->_id) ? "object" : "actor",
				commonObjectData->_id, frameNumber);
		return false;
	}
	return true;
}

void Actor::drawActors() {
	// Do nothing for SAGA2 games for now
	if (_vm->isSaga2()) {
		return;
	}

	if (_vm->_anim->hasCutaway()) {
		drawSpeech();
		return;
	}

	if (_vm->_scene->currentSceneNumber() <= 0) {
		return;
	}

	if (_vm->_scene->_entryList.entryListCount == 0) {
		return;
	}

	CommonObjectOrderList::iterator drawOrderIterator;
	CommonObjectDataPointer drawObject;
	int frameNumber = 0;
	SpriteList *spriteList = NULL;

	createDrawOrderList();

	for (drawOrderIterator = _drawOrderList.begin(); drawOrderIterator != _drawOrderList.end(); ++drawOrderIterator) {
		drawObject = *drawOrderIterator;

		if (!getSpriteParams(drawObject, frameNumber, spriteList)) {
			continue;
		}

		if (_vm->_scene->getFlags() & kSceneFlagISO) {
			_vm->_isoMap->drawSprite(*spriteList, frameNumber, drawObject->_location, drawObject->_screenPosition, drawObject->_screenScale);
		} else {
			_vm->_sprite->drawOccluded(*spriteList, frameNumber, drawObject->_screenPosition, drawObject->_screenScale, drawObject->_screenDepth);
		}
	}

	drawSpeech();
}

void Actor::drawSpeech(void) {
	if (!isSpeaking() || !_activeSpeech.playing || _vm->_script->_skipSpeeches
		|| (!_vm->_subtitlesEnabled && _vm->getGameId() == GID_ITE && !(_vm->getFeatures() & GF_ITE_FLOPPY))
		|| (!_vm->_subtitlesEnabled && (_vm->getGameId() == GID_IHNM)))
		return;

	Point textPoint;
	ActorData *actor;
	int width, height;
	int stringLength = strlen(_activeSpeech.strings[0]);
	char *outputString = (char*)calloc(stringLength + 1, 1);

	if (_activeSpeech.speechFlags & kSpeakSlow)
		strncpy(outputString, _activeSpeech.strings[0], _activeSpeech.slowModeCharIndex + 1);
	else
		strncpy(outputString, _activeSpeech.strings[0], stringLength);

	if (_activeSpeech.actorsCount > 1) {
		height = _vm->_font->getHeight(kKnownFontScript);
		width = _vm->_font->getStringWidth(kKnownFontScript, _activeSpeech.strings[0], 0, kFontNormal);

		for (int i = 0; i < _activeSpeech.actorsCount; i++) {
			actor = getActor(_activeSpeech.actorIds[i]);
			calcScreenPosition(actor);

			textPoint.x = CLIP(actor->_screenPosition.x - width / 2, 10, _vm->getDisplayInfo().width - 10 - width);

			if (_vm->getGameId() == GID_ITE)
				textPoint.y = CLIP(actor->_screenPosition.y - 58, 10, _vm->_scene->getHeight(true) - 10 - height);
			else if (_vm->getGameId() == GID_IHNM)
				textPoint.y = 10; // CLIP(actor->_screenPosition.y - 160, 10, _vm->_scene->getHeight(true) - 10 - height);

			_vm->_font->textDraw(kKnownFontScript, outputString, textPoint,
				_activeSpeech.speechColor[i], _activeSpeech.outlineColor[i], _activeSpeech.getFontFlags(i));
		}
	} else {
		_vm->_font->textDrawRect(kKnownFontScript, outputString, _activeSpeech.drawRect, _activeSpeech.speechColor[0],
			_activeSpeech.outlineColor[0], _activeSpeech.getFontFlags(0));
	}

	free(outputString);
}

void Actor::actorSpeech(uint16 actorId, const char **strings, int stringsCount, int sampleResourceId, int speechFlags) {
	ActorData *actor;
	int i;
	int16 dist;

	actor = getActor(actorId);
	calcScreenPosition(actor);
	for (i = 0; i < stringsCount; i++) {
		_activeSpeech.strings[i] = strings[i];
	}

	_activeSpeech.stringsCount = stringsCount;
	_activeSpeech.speechFlags = speechFlags;
	_activeSpeech.actorsCount = 1;
	_activeSpeech.actorIds[0] = actorId;
	_activeSpeech.speechColor[0] = actor->_speechColor;
	_activeSpeech.outlineColor[0] = _vm->KnownColor2ColorId(kKnownColorBlack);
	_activeSpeech.sampleResourceId = sampleResourceId;
	_activeSpeech.playing = false;
	_activeSpeech.slowModeCharIndex = 0;

	dist = MIN(actor->_screenPosition.x - 10, _vm->getDisplayInfo().width - 10 - actor->_screenPosition.x);

	if (_vm->getGameId() == GID_ITE)
		dist = CLIP<int16>(dist, 60, 150);
	else
		dist = CLIP<int16>(dist, 120, 300);

	_activeSpeech.speechBox.left = actor->_screenPosition.x - dist;
	_activeSpeech.speechBox.right = actor->_screenPosition.x + dist;

	if (_activeSpeech.speechBox.left < 10) {
		_activeSpeech.speechBox.right += 10 - _activeSpeech.speechBox.left;
		_activeSpeech.speechBox.left = 10;
	}
	if (_activeSpeech.speechBox.right > _vm->getDisplayInfo().width - 10) {
		_activeSpeech.speechBox.left -= _activeSpeech.speechBox.right - _vm->getDisplayInfo().width - 10;
		_activeSpeech.speechBox.right = _vm->getDisplayInfo().width - 10;
	}

	// HACK for the compact disk in Ellen's chapter
	// Once Ellen starts saying that "Something is different", bring the compact disk in the
	// scene. After speaking with AM, the compact disk is visible. She always says this line
	// when entering room 59, after speaking with AM, if the compact disk is not picked up yet
	// Check Script::sfDropObject for the other part of this hack
	if (_vm->getGameId() == GID_IHNM && _vm->_scene->currentChapterNumber() == 3 &&
		_vm->_scene->currentSceneNumber() == 59 && _activeSpeech.sampleResourceId == 286) {
		for (i = 0; i < _objsCount; i++) {
			if (_objs[i]->_id == 16385) {	// the compact disk
				_objs[i]->_sceneNumber = 59;
				break;
			}
		}
	}

}

void Actor::nonActorSpeech(const Common::Rect &box, const char **strings, int stringsCount, int sampleResourceId, int speechFlags) {
	int i;

	_vm->_script->wakeUpThreads(kWaitTypeSpeech);

	for (i = 0; i < stringsCount; i++) {
		_activeSpeech.strings[i] = strings[i];
	}
	_activeSpeech.stringsCount = stringsCount;
	_activeSpeech.speechFlags = speechFlags;
	_activeSpeech.actorsCount = 1;
	_activeSpeech.actorIds[0] = 0;
	if (_vm->getFeatures() & GF_ITE_FLOPPY)
		_activeSpeech.sampleResourceId = -1;
	else
		_activeSpeech.sampleResourceId = sampleResourceId;
	_activeSpeech.playing = false;
	_activeSpeech.slowModeCharIndex = 0;
	_activeSpeech.speechBox = box;
}

void Actor::simulSpeech(const char *string, uint16 *actorIds, int actorIdsCount, int speechFlags, int sampleResourceId) {
	int i;

	for (i = 0; i < actorIdsCount; i++) {
		ActorData *actor;

		actor = getActor(actorIds[i]);
		_activeSpeech.actorIds[i] = actorIds[i];
		_activeSpeech.speechColor[i] = actor->_speechColor;
		_activeSpeech.outlineColor[i] = _vm->KnownColor2ColorId(kKnownColorBlack);
	}
	_activeSpeech.actorsCount = actorIdsCount;
	_activeSpeech.strings[0] = string;
	_activeSpeech.stringsCount = 1;
	_activeSpeech.speechFlags = speechFlags;
	_activeSpeech.sampleResourceId = sampleResourceId;
	_activeSpeech.playing = false;
	_activeSpeech.slowModeCharIndex = 0;

	// caller should call thread->wait(kWaitTypeSpeech) by itself
}

void Actor::abortAllSpeeches() {
	// WORKAROUND: Don't abort speeches in scene 31 (tree with beehive). This prevents the
	// making fire animation from breaking
	if (_vm->getGameId() == GID_ITE && _vm->_scene->currentSceneNumber() == 31)
		return;

	abortSpeech();

	if (_vm->_script->_abortEnabled)
		_vm->_script->_skipSpeeches = true;

	for (int i = 0; i < 10; i++)
		_vm->_script->executeThreads(0);
}

void Actor::abortSpeech() {
	_vm->_sound->stopVoice();
	_activeSpeech.playingTime = 0;
}

void Actor::saveState(Common::OutSaveFile *out) {
	uint16 i;

	out->writeSint16LE(getProtagState());

	for (i = 0; i < _actorsCount; i++) {
		ActorData *a = _actors[i];
		a->saveState(out);
	}

	for (i = 0; i < _objsCount; i++) {
		ObjectData *o = _objs[i];
		o->saveState(out);
	}
}

void Actor::loadState(Common::InSaveFile *in) {
	int32 i;

	int16 protagState = in->readSint16LE();
	if (protagState != 0 || _protagonist->_shareFrames)
		setProtagState(protagState);

	for (i = 0; i < _actorsCount; i++) {
		ActorData *a = _actors[i];
		a->loadState(_vm->getCurrentLoadVersion(), in);
	}

	for (i = 0; i < _objsCount; i++) {
		ObjectData *o = _objs[i];
		o->loadState(in);
	}
}

} // End of namespace Saga
