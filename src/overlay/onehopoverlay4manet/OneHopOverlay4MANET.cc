//
// Copyright (C) 2006 Institut fuer Telematik, Universitaet Karlsruhe (TH)
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//

/**
 * @file OneHopOverlay4MANET.cc
 * @author Frank Ockenfeld
 * based on EpiChord.cc, by Jamie Furness
 */

#include <GlobalStatistics.h>
#include <BootstrapList.h>
#include <IterativeLookup.h>

#include "OneHopOverlay4MANETNodeList.h"
#include "OneHopOverlay4MANETCache.h"
#include "OneHopOverlay4MANET.h"
#include "OneHopOverlay4MANETIterativeLookup.h"
#include <ChurnGenerator.h>

#include <NotificationBoard.h>
#include "NotifierConsts.h"
#include "SimpleInfo.h"

// To convert between IP addresses (which have bit 24 active), and keys (which don't), we'll need to set or remove this bit.
#define BIGBIT (1 << 24)

namespace oversim {
using namespace std;

Define_Module(OneHopOverlay4MANET);

OneHopOverlay4MANET::OneHopOverlay4MANET()
{
    notificationBoard = NULL;
}

OneHopOverlay4MANET::~OneHopOverlay4MANET()
{
	// destroy self timer messages
	cancelAndDelete(join_timer);
	cancelAndDelete(stabilize_timer);
	cancelAndDelete(cache_timer);
}

void OneHopOverlay4MANET::initializeOverlay(int stage)
{
    if (stage == MAX_STAGE_OVERLAY)
    {
    }

	// because of IPAddressResolver, we need to wait until interfaces
	// are registered, address auto-assignment takes place etc.
	if (stage != MIN_STAGE_OVERLAY)
		return;

    // get a pointer to the NotificationBoard module
	    notificationBoard = NotificationBoardAccess().get();

	if (defaultRoutingType != ITERATIVE_ROUTING && defaultRoutingType != EXHAUSTIVE_ITERATIVE_ROUTING)
		throw new cRuntimeError("OneHopOverlay4MANET::initializeOverlay(): OneHopOverlay4MANET only works with iterative routing.");

	if (iterativeLookupConfig.redundantNodes < 2)
		throw new cRuntimeError("OneHopOverlay4MANET::initializeOverlay(): OneHopOverlay4MANET requires lookupRedundantNodes >= 2.");

	// OneHopOverlay4MANET provides KBR services
	kbr = true;

	// fetch some parameters
	successorListSize = par("successorListSize");
	nodesPerSlice = par("nodesPerSlice");
	joinRetry = par("joinRetry");
	joinDelay = par("joinDelay");
	stabilizeDelay = par("stabilizeDelay");
	stabilizeEstimation = par("stabilizeEstimation");
	stabilizeEstimateMuliplier = par("stabilizeEstimateMuliplier");
	cacheFlushDelay = par("cacheFlushDelay");
	cacheCheckMultiplier = par("cacheCheckMultiplier");
	cacheTTL = par("cacheTTL");
	cacheUpdateDelta = par("cacheUpdateDelta");
	activePropagation = par("activePropagation");
	sendFalseNegWarnings = par("sendFalseNegWarnings");
	fibonacci = par("fibonacci");

	//receiveNode bools
    SuccAdded = false;
    PreAdded = false;

	// statistics
	joinCount = 0;
	joinBytesSent = 0;
	stabilizeCount = 0;
	stabilizeBytesSent = 0;

	nodeProbes = 0;
	nodeTimeouts = 0;
	cacheCheckCounter = 0;

	// find friend modules
	findFriendModules();

	// add some watches
	WATCH(thisNode);
	//WATCH(bootstrapNode);
	WATCH(joinRetry);

	// self-messages
	join_timer = new cMessage("join_timer");
	stabilize_timer = new cMessage("stabilize_timer");
	cache_timer = new cMessage("cache_timer");

    // subscribe to NotificationBoard
    notificationBoard->subscribe(this, NF_IPv4_ROUTE_ADDED);
    notificationBoard->subscribe(this, NF_IPv4_ROUTE_DELETED);
    notificationBoard->subscribe(this, NF_IPv4_ROUTE_CHANGED);

    notificationBoard->subscribe(this, NF_IPv4_MROUTE_ADDED);
    notificationBoard->subscribe(this, NF_IPv4_MROUTE_DELETED);
    notificationBoard->subscribe(this, NF_IPv4_MROUTE_CHANGED);
}

//Processes "timer" self-messages
void OneHopOverlay4MANET::handleTimerEvent(cMessage* msg)
{
	// catch JOIN timer
	if (msg == join_timer)
		handleJoinTimerExpired(msg);
	// catch STABILIZE timer
	else if (msg == stabilize_timer)
		handleStabilizeTimerExpired(msg);
	// catch CACHE FLUSH timer
	else if (msg == cache_timer)
		handleCacheFlushTimerExpired(msg);
	else
		error("OneHopOverlay4MANET::handleTimerEvent(): received self message of unknown type!");
}

//record method
void OneHopOverlay4MANET::recordOverlaySentStats(BaseOverlayMessage* msg)
{
	BaseOverlayMessage* innerMsg = msg;
	while (innerMsg->getType() != APPDATA && innerMsg->getEncapsulatedPacket() != NULL)
		innerMsg = static_cast<BaseOverlayMessage*>(innerMsg->getEncapsulatedPacket());

	switch (innerMsg->getType()) {
		case OVERLAYSIGNALING: {
			throw new cRuntimeError("Unknown overlay signaling message type!");
		}

		case RPC: {
			//nodeProbes++;

			// It was a stabilize call/response
			if ((dynamic_cast<OneHopOverlay4MANETStabilizeCall*>(innerMsg) != NULL) || (dynamic_cast<OneHopOverlay4MANETStabilizeResponse*>(innerMsg) != NULL))
				RECORD_STATS(stabilizeCount++; stabilizeBytesSent += msg->getByteLength());
			// It was a join call/response
			else if ((dynamic_cast<OneHopOverlay4MANETJoinCall*>(innerMsg) != NULL) || (dynamic_cast<OneHopOverlay4MANETJoinResponse*>(innerMsg) != NULL))
				RECORD_STATS(joinCount++; joinBytesSent += msg->getByteLength());
			break;
		}

		case APPDATA: {
			break;
		}

		default: {
			throw new cRuntimeError("Unknown message type!");
		}
	}
}

//some stats after simulation ended
void OneHopOverlay4MANET::finishOverlay()
{
	simtime_t time = globalStatistics->calcMeasuredLifetime(creationTime);
	if (time < GlobalStatistics::MIN_MEASURED)
		return;

	globalStatistics->addStdDev("OneHopOverlay4MANET: Sent JOIN count", joinCount / time);
	//globalStatistics->addStdDev("OneHopOverlay4MANET: Sent STABILIZE Messages/s", stabilizeCount / time);
	//globalStatistics->addStdDev("OneHopOverlay4MANET: Sent JOIN Bytes/s", joinBytesSent / time);
	//globalStatistics->addStdDev("OneHopOverlay4MANET: Sent STABILIZE Bytes/s", stabilizeBytesSent / time);
	//globalStatistics->addStdDev("OneHopOverlay4MANET: Cache updates success/s", cache->getSuccessfulUpdates() / time);

	globalStatistics->addStdDev("OneHopOverlay4MANET: Cache live nodes", cache->countLive());
	//globalStatistics->addStdDev("OneHopOverlay4MANET: Cache live nodes (real)", cache->countRealLive());
	globalStatistics->addStdDev("OneHopOverlay4MANET: Cache dead nodes", cache->countDead());
	//globalStatistics->addStdDev("OneHopOverlay4MANET: Cache dead nodes (real)", cache->countRealDead());

	// Estimated node lifetime
	if (stabilizeEstimation)
		globalStatistics->addStdDev("OneHopOverlay4MANET: Estimated node lifetime", SIMTIME_DBL(cache->estimateNodeLifetime()));
}

//calculates the distance between two node clockwise
OverlayKey OneHopOverlay4MANET::distance(const OverlayKey& x, const OverlayKey& y, bool useAlternative) const
{
	return KeyRingMetric().distance(x, y);
}

//updates simulation tooltips
void OneHopOverlay4MANET::updateTooltip()
{
	if (ev.isGUI()) {
		std::stringstream ttString;

		// show our predecessor and successor in tooltip
		ttString << predecessorList->getNode() << endl << thisNode << endl << successorList->getNode();

		getParentModule()->getParentModule()->getDisplayString().
		setTagArg("tt", 0, ttString.str().c_str());
		getParentModule()->getDisplayString().
		setTagArg("tt", 0, ttString.str().c_str());
		getDisplayString().setTagArg("tt", 0, ttString.str().c_str());

		// draw an arrow to our current successor
		showOverlayNeighborArrow(successorList->getNode(), true, "m=m,50,0,50,0;ls=red,1");
		showOverlayNeighborArrow(predecessorList->getNode(), false, "m=m,50,100,50,100;ls=green,1");
	}
}

//find correct cache in init phase
void OneHopOverlay4MANET::findFriendModules()
{
	successorList = check_and_cast<OneHopOverlay4MANETNodeList*>(getParentModule()->getSubmodule("successorList"));
	predecessorList = check_and_cast<OneHopOverlay4MANETNodeList*>(getParentModule()->getSubmodule("predecessorList"));
	cache = check_and_cast<OneHopOverlay4MANETCache*>(getParentModule()->getSubmodule("Cache"));
}

//init caches
void OneHopOverlay4MANET::initializeFriendModules()
{
	// initialize Cache
	cache->initializeCache(thisNode, this, cacheTTL);

	// initialize successor list
	successorList->initializeList(successorListSize, thisNode, cache, this, true);

	// initialize predecessor list
	predecessorList->initializeList(successorListSize, thisNode, cache, this, false);
}

//defines what to do in the 3 different states (init, join, ready)
void OneHopOverlay4MANET::changeState(int toState)
{
	// Defines tasks to be executed when a state change occurs.
	switch (toState) {
	case INIT:
		state = INIT;

		setOverlayReady(false);

		// initialize successor and predecessor lists
		initializeFriendModules();

		updateTooltip();

		// debug message
		if (debugOutput) {
			EV << "[OneHopOverlay4MANET::changeState() @ " << thisNode.getIp()
			<< " (" << thisNode.getKey().toString(16) << ")]\n"
			<< "	Entered INIT stage"
			<< endl;
		}

        // initiate stabilize process
        cancelEvent(stabilize_timer);
        scheduleAt(simTime() + stabilizeDelay, stabilize_timer);

		getParentModule()->getParentModule()->bubble("Enter INIT state.");
		break;

	case JOIN:
		state = JOIN;

		setOverlayReady(true);

        // initiate cache flush protocol for old entries
        cancelEvent(cache_timer);
        scheduleAt(simTime() + cacheFlushDelay, cache_timer);

		// debug message
		if (debugOutput) {
			EV << "[OneHopOverlay4MANET::changeState() @ " << thisNode.getIp()
			<< " (" << thisNode.getKey().toString(16) << ")]\n"
			<< "	Entered JOIN stage"
			<< endl;
		}
		getParentModule()->getParentModule()->bubble("Enter JOIN state.");
		break;

	case READY:
		state = READY;

		//setOverlayReady(true);

        // initiate join process
        cancelEvent(join_timer);
        // workaround: prevent notificationBoard from taking
        // ownership of join_timer message
        take(join_timer);
        scheduleAt(simTime() + joinDelay, join_timer);


		// debug message
		if (debugOutput) {
			EV << "[OneHopOverlay4MANET::changeState() @ " << thisNode.getIp()
			<< " (" << thisNode.getKey().toString(16) << ")]\n"
			<< "	Entered READY stage"
			<< endl;
		}
		getParentModule()->getParentModule()->bubble("Enter READY state.");
		break;
	}
}

//handles join overlay timeout and handles further actions
void OneHopOverlay4MANET::handleJoinTimerExpired(cMessage* msg)
{
	// only process timer if node is ready (waited for stabilize_timer)
	if (state != READY)
	    return;

	// join retry
	if (joinRetry >= 0) {
	    joinRetry--;
		changeState(JOIN);
	}

    //try to join overlay
    //SimpleInfo* info;
    //info = new SimpleInfo(0, getParentModule()->getParentModule()->getId(), NULL);
    //joiningNode.setDestination(IPv4Address(thisNode.getIp().get4().getInt() & ~BIGBIT));
    //globalNodeList->addPeer(thisNode.getIp(),info);
    globalNodeList->registerPeer(thisNode, overlayId);

	// schedule next join process in the case it failed
	cancelEvent(join_timer);
	scheduleAt(simTime() + truncnormal(joinDelay,joinDelay*0.1), msg);
}

//handles stabilization timeout and set node to ready to join overlay
void OneHopOverlay4MANET::handleStabilizeTimerExpired(cMessage* msg)
{
	if (state != INIT)
		return;
	changeState(READY);

	//let first nodes join after stabilize_timer triggered, rejoin 10s after it failed to join the overlay
	cancelEvent(join_timer);
	scheduleAt(simTime(), join_timer);

	// Decide when to next schedule stabilization
	simtime_t avgLifetime = stabilizeDelay;
	if (stabilizeEstimation) {
		simtime_t estimate = cache->estimateNodeLifetime();
		if (estimate > 0)
			avgLifetime = estimate * stabilizeEstimateMuliplier;
	}

	// schedule next stabilization process
	cancelEvent(stabilize_timer);
	scheduleAt(simTime() + avgLifetime, msg);
}

//flushes "old" entries
void OneHopOverlay4MANET::handleCacheFlushTimerExpired(cMessage* msg)
{
	if (state != JOIN)
		return;

	// Remove expired entries from the cache
	cache->removeOldEntries();

    // Remove expired entries from Succ-, PredecessorList
    //successorList->removeOldEntries(simTime(), cacheTTL);
    //predecessorList->removeOldEntries(simTime(), cacheTTL);

	// schedule next cache flush process
	cancelEvent(cache_timer);
	scheduleAt(simTime() + cacheFlushDelay, msg);
}

//find a key
NodeVector* OneHopOverlay4MANET::findNode(const OverlayKey& key, int numRedundantNodes, int numSiblings, BaseOverlayMessage* msg)
{
	OneHopOverlay4MANETFindNodeExtMessage* findNodeExt = NULL;

	if (msg != NULL) {
		if (!msg->hasObject("findNodeExt")) {
			findNodeExt = new OneHopOverlay4MANETFindNodeExtMessage("findNodeExt");
			msg->addObject(findNodeExt);
		}
		else
			findNodeExt = (OneHopOverlay4MANETFindNodeExtMessage*) msg->getObject("findNodeExt");

		// Reset the expires array to 0 in case we return before setting the new size
		findNodeExt->setLastUpdatesArraySize(0);
	}

	NodeVector* nextHop = new NodeVector();

	if (state != JOIN)
		return nextHop;

	simtime_t now = simTime();
	NodeHandle source = NodeHandle::UNSPECIFIED_NODE;
	std::vector<simtime_t>* lastUpdates = new std::vector<simtime_t>();
	std::set<NodeHandle>* exclude = new std::set<NodeHandle>();
	bool err;

	exclude->insert(thisNode);

	if (msg != NULL) {
		// Add the origin node to the cache
		source = ((FindNodeCall*) msg)->getSrcNode();
		if (!source.isUnspecified())
			exclude->insert(source);

		this->receiveNewNode(source, true, OBSERVED, now);
	}

	// if the message is destined for this node
	if (key.isUnspecified() || isSiblingFor(thisNode, key, 1, &err)) {
		nextHop->push_back(thisNode);
		lastUpdates->push_back(now);

		if (!predecessorList->isEmpty()) {
			OneHopOverlay4MANETCacheEntry* entry = cache->getNode(predecessorList->getNode());
			nextHop->push_back(predecessorList->getNode());
			if (entry != NULL)
				lastUpdates->push_back(entry->lastUpdate);
			else
				lastUpdates->push_back(now);
		}

		if (!successorList->isEmpty()) {
			OneHopOverlay4MANETCacheEntry* entry = cache->getNode(successorList->getNode());
			nextHop->push_back(successorList->getNode());
			if (entry != NULL)
				lastUpdates->push_back(entry->lastUpdate);
			else
				lastUpdates->push_back(now);
		}
	}
	else {
		const OneHopOverlay4MANETCacheEntry* entry;

		// No source specified, this implies it is a local request
		if (source.isUnspecified()) {
			OverlayKey successorDistance = distance(successorList->getNode().getKey(), key);
			OverlayKey predecessorDistance = distance(predecessorList->getNode().getKey(), key);

			// add a successor or predecessor, depending on which one is closer to the target
			entry = cache->getNode(predecessorDistance < successorDistance ? predecessorList->getNode() : successorList->getNode());
		}
		//   ---- (source) ---- (us) ---- (destination) ----
		else if (thisNode.getKey().isBetween(source.getKey(), key)) {
			// add a successor
			entry = cache->getNode(successorList->getNode());
		}
		// ---- (destination) ---- (us) ---- (source) ----
		else {
			// add a predecessor
			entry = cache->getNode(predecessorList->getNode());
		}

		if (entry != NULL) {
			nextHop->push_back(entry->nodeHandle);
			lastUpdates->push_back(entry->lastUpdate);
			exclude->insert(entry->nodeHandle);
		}

		// Add the numRedundantNodes best next hops
		cache->findBestHops(key, nextHop, lastUpdates, exclude, numRedundantNodes);
	}

	// Check we managed to actually find something
	if (nextHop->empty())
		throw new cRuntimeError("OneHopOverlay4MANET::findNode() Failed to find node");

	if (msg != NULL) {
		int numVisited = nextHop->size();
		findNodeExt->setLastUpdatesArraySize(numVisited);

		for (int i = 0;i < numVisited;i++)
			findNodeExt->setLastUpdates(i, (*lastUpdates)[i]);

		findNodeExt->setBitLength(ONEHOPOVERLAY4MANET_FINDNODEEXTMESSAGE_L(findNodeExt));
	}

	delete exclude;
	delete lastUpdates;
	return nextHop;
}

//change state to INIT and join to join overlay
void OneHopOverlay4MANET::joinOverlay()
{
    if(state != JOIN)
        changeState(INIT);
}

//Query if a node is among the siblings for a given key. (needed for lookups)
bool OneHopOverlay4MANET::isSiblingFor(const NodeHandle& node, const OverlayKey& key, int numSiblings, bool* err)
{
	assert(!key.isUnspecified());

	if (state != JOIN) {
		*err = true;
		return false;
	}

	// set default number of siblings to consider
	if (numSiblings < 0 || numSiblings > this->getMaxNumSiblings())
		numSiblings = this->getMaxNumSiblings();

	// if this is the first and only node on the ring, it is responsible
	if (predecessorList->isEmpty() && node == thisNode) {
		if (successorList->isEmpty() || node.getKey() == key) {
			*err = false;
			return true;
		}
		else {
			*err = true;
			return false;
		}
	}

	// if its between our predecessor and us, it's for us
	if (node == thisNode && key.isBetweenR(predecessorList->getNode().getKey(), thisNode.getKey())) {
		*err = false;
		return true;
	}

	NodeHandle prevNode = predecessorList->getNode();
	NodeHandle curNode;

	for (int i = -1;i < (int)successorList->getSize();i++) {
		curNode = i < 0 ? thisNode : successorList->getNode(i);

		if (node == curNode) {
			// is the message destined for curNode?
			if (key.isBetweenR(prevNode.getKey(), curNode.getKey())) {
				if (numSiblings <= ((int)successorList->getSize() - i)) {
					*err = false;
					return true;
				}
				else {
					*err = true;
					return false;
				}
			}
			else {
				// the key doesn't directly belong to this node, but
				// the node could be a sibling for this key
				if (numSiblings <= 1) {
					*err = false;
					return false;
				}
				else {
					// In OneHopOverlay4MANET we don't know if we belong to the
					// replicaSet of one of our predecessors
					*err = true;
					return false;
				}
			}
		}
		prevNode = curNode;
	}

	// node is not in our neighborSet
	*err = true;
	return false;
}

//returns Successor and Predecessor list size
int OneHopOverlay4MANET::getMaxNumSiblings()
{
	return successorListSize;
}

int OneHopOverlay4MANET::getMaxNumRedundantNodes()
{
	return iterativeLookupConfig.redundantNodes;
}

//returns lookup ratio failures
double OneHopOverlay4MANET::calculateGamma()
{
	double gamma = 0.0; // ratio of lookup failures

	// Make sure we don't divide by 0!
	if (nodeProbes > 0)
		gamma = nodeTimeouts / nodeProbes;

	return gamma;
}

//helper for IterativeLookup call
bool OneHopOverlay4MANET::handleRpcCall(BaseCallMessage* msg)
{
	if (state != JOIN) {
		EV << "[OneHopOverlay4MANET::handleRpcCall() @ " << thisNode.getIp()
		   << " (" << thisNode.getKey().toString(16) << ")]\n"
		   << "	Received RPC call and state != JOIN"
		   << endl;
		return false;
	}

	// delegate messages
	RPC_SWITCH_START(msg)

	RPC_DELEGATE(OneHopOverlay4MANETFalseNegWarning, rpcFalseNegWarning);

	RPC_SWITCH_END()

	return RPC_HANDLED;
}

//helper for IterativeLookup response
void OneHopOverlay4MANET::handleRpcResponse(BaseResponseMessage* msg, cPolymorphic* context, int rpcId, simtime_t rtt)
{
	RPC_SWITCH_START(msg)

	RPC_ON_RESPONSE(FindNode) {
		handleRpcFindNodeResponse(_FindNodeResponse);
		EV << "[OneHopOverlay4MANET::handleRpcResponse() @ " << thisNode.getIp()
		<< " (" << thisNode.getKey().toString(16) << ")]\n"
		<< "	Received a FindNode Response: id=" << rpcId << "\n"
		<< "	msg=" << *_FindNodeResponse << " rtt=" << rtt
		<< endl;
		break;
	}

	RPC_SWITCH_END( )
}

//helper for IterativeLookup if lookup timed out
void OneHopOverlay4MANET::handleRpcTimeout(BaseCallMessage* msg, const TransportAddress& dest, cPolymorphic* context, int rpcId, const OverlayKey&)
{
	nodeTimeouts++;

	// Handle failed node
	if (!dest.isUnspecified() && !handleFailedNode(dest))
		join();
}

//This method is called whenever a node given by findNode() was unreachable
bool OneHopOverlay4MANET::handleFailedNode(const TransportAddress& failed)
{
	Enter_Method_Silent();

	assert(failed != thisNode);

	successorList->handleFailedNode(failed);
	predecessorList->handleFailedNode(failed);
	cache->handleFailedNode(failed);

	if (activePropagation && (predecessorList->hasChanged() || successorList->hasChanged())) {
		// schedule next stabilization process
		cancelEvent(stabilize_timer);
		scheduleAt(simTime(), stabilize_timer);

		updateTooltip();
	}

	if (state != JOIN)
		return true;

	// lost our last successor - cancel periodic stabilize tasks and wait for rejoin
	if (successorList->isEmpty() || predecessorList->isEmpty()) {
		cancelEvent(stabilize_timer);
		cancelEvent(cache_timer);

		return false;
	}

	return true;
}

void OneHopOverlay4MANET::sendFalseNegWarning(NodeHandle bestPredecessor, NodeHandle bestSuccessor, NodeVector* deadNodes)
{
	Enter_Method_Silent();

	if (state != JOIN)
		return;

	// If we aren't to send warnings then stop
	if (!sendFalseNegWarnings)
		return;

	OneHopOverlay4MANETFalseNegWarningCall* warning = new OneHopOverlay4MANETFalseNegWarningCall("OneHopOverlay4MANETFalseNegWarningCall");

	warning->setBestPredecessor(bestPredecessor);

	warning->setDeadNodeArraySize(deadNodes->size());
	for (uint i = 0;i < deadNodes->size();i++)
		warning->setDeadNode(i, (*deadNodes)[i]);

	warning->setBitLength(ONEHOPOVERLAY4MANET_FALSENEGWARNINGCALL_L(warning));
	sendUdpRpcCall(bestSuccessor, warning);
}

//rpc False Negation responds
void OneHopOverlay4MANET::rpcFalseNegWarning(OneHopOverlay4MANETFalseNegWarningCall* warning)
{
	NodeHandle oldPredecessor = predecessorList->getNode();
	NodeHandle bestPredecessor = warning->getBestPredecessor();

	// Handle this warning!
	if (oldPredecessor != bestPredecessor) {
		// Remove all dead nodes
		int deadNum = warning->getDeadNodeArraySize();
		for (int i = 0;i < deadNum;i++)
			handleFailedNode(warning->getDeadNode(i));

		// Ensure the predecessor is known to us
		predecessorList->addNode(bestPredecessor);

		// If there were any changes, and they effected us
		if (activePropagation && (predecessorList->hasChanged() || successorList->hasChanged())) {
			// schedule next stabilization process
			cancelEvent(stabilize_timer);
			scheduleAt(simTime(), stabilize_timer);

			updateTooltip();
		}
	}

	OneHopOverlay4MANETFalseNegWarningResponse* response = new OneHopOverlay4MANETFalseNegWarningResponse("OneHopOverlay4MANETFalseNegWarningResponse");

	// Send the response
	response->setBitLength(ONEHOPOVERLAY4MANET_FALSENEGWARNINGRESPONSE_L(response));
	sendRpcResponse(warning, response);
}

//creates a node lookup call
AbstractLookup* OneHopOverlay4MANET::createLookup(RoutingType routingType, const BaseOverlayMessage* msg, const cPacket* findNodeExt, bool appLookup)
{
	assert(findNodeExt == NULL);

	// Create a new OneHopOverlay4MANETFindNodeExtMessage
	findNodeExt = new OneHopOverlay4MANETFindNodeExtMessage("findNodeExt");

	AbstractLookup* newLookup = new OneHopOverlay4MANETIterativeLookup(this, routingType, iterativeLookupConfig, findNodeExt, appLookup);

	delete findNodeExt;

	lookups.insert(newLookup);
	return newLookup;
}

//response to FindNode callback
void OneHopOverlay4MANET::handleRpcFindNodeResponse(FindNodeResponse* response)
{
	if (!response->hasObject("findNodeExt"))
		return;

	OneHopOverlay4MANETFindNodeExtMessage* findNodeExt = (OneHopOverlay4MANETFindNodeExtMessage*) response->getObject("findNodeExt");
	assert(response->getClosestNodesArraySize() == findNodeExt->getLastUpdatesArraySize());

	// Take a note of all nodes returned in this FindNodeResponse
	int nodeNum = response->getClosestNodesArraySize();
	for (int i = 0;i < nodeNum;i++)
		this->receiveNewNode(response->getClosestNodes(i), false, OBSERVED, findNodeExt->getLastUpdates(i));
}

//attempt to add node into any cache
void OneHopOverlay4MANET::receiveNewNode(const NodeHandle& node, bool direct, NodeSource source, simtime_t lastUpdate)
{
	if (node.isUnspecified())
		return;

	/**
    // if we don't have a successor, the requester is also our new successor
    if (successorList->isEmpty()){
        successorList->addNode(joinAck->getSrcNode());
    }

    // he is now our predecessor
    predecessorList->addNode(joinAck->getSrcNode());*/

	// Attempt to add to successor list
	if (!successorList->contains(node) && (!successorList->isFull() || node.getKey().isBetween(thisNode.getKey(), successorList->getNode(successorList->getSize() - 1).getKey()))) {
		if (direct){
			//successorList->addNode(node, true);
		    successorList->addNode2(node, simTime(), true);
		    SuccAdded = true;
		}
	}
	//update cacheTTL if node already exists
	else if (successorList->contains(node)){
	    successorList->updateNode(node, simTime());
	    SuccAdded = true;
	}
	//node was not added nor updated
	else
	    SuccAdded = false;

	// Attempt to add to predecessor list
	if (!predecessorList->contains(node) && (!predecessorList->isFull() || node.getKey().isBetween(predecessorList->getNode(predecessorList->getSize() - 1).getKey(), thisNode.getKey()))) {
		if (direct){
			//predecessorList->addNode(node, true);
		    predecessorList->addNode2(node, simTime(), true);
		    PreAdded = true;
		 }
	}   //update cacheTTL if node already exists
    else if (predecessorList->contains(node)){
        predecessorList->updateNode(node, simTime());
        PreAdded = true;
    }
    //node was not added nor updated
	else PreAdded = false;

	//add node to Cache if wasn't added to succ- nor predecessor List
	if(SuccAdded == false && PreAdded == false){
	    cache->updateEntry(node, direct, lastUpdate, cacheTTL, source);
	}

    updateTooltip();

    //changeState(READY);

	// Attempt to add to update successor list
	/**
	// if we don't have any predecessors, the requester is also our new predecessor
	    if (predecessorList->isEmpty())
	        predecessorList->addNode(joinResponse->getSrcNode());*/

    	// If there were any changes, and they effected us
	/*if (activePropagation && (predecessorList->hasChanged() || successorList->hasChanged())) {
		// schedule next stabilization process
		cancelEvent(stabilize_timer);
		scheduleAt(simTime(), stabilize_timer);

		updateTooltip();
	}*/
}

void OneHopOverlay4MANET::setOwnNodeID(){
    thisNode.setKey(OverlayKey::sha1(thisNode.getIp().get4().getInt()));
}

void OneHopOverlay4MANET::receiveChangeNotification(int category, const cObject *details)
{
    // ignore notifications during initialize phase
    if (simulation.getContextType()==CTX_INITIALIZE || state != JOIN)
        return;

    Enter_Method_Silent();
    //printNotificationBanner(category, details);

    //cast cObject *details into something useful
    IPv4Route *route = const_cast<IPv4Route*>(check_and_cast<const IPv4Route*>(details));

    // ignore notification if thisNode or received node is not a peer yet
        if (globalNodeList->getNodeHandle(IPvXAddress(route->getDestination())) == NULL || globalNodeList->getNodeHandle(thisNode.getIp()) == NULL)
            return;

    //Route destination node
        //route->getMetric(); //distance between source and destination node
    Destination.setIp(IPvXAddress(route->getDestination()),thisNode.getPort(),thisNode.getNatType());
    //Destination.setAddress(IPvXAddress(route->getDestination()),thisNode.getPort(),thisNode.getNatType());
        //Destination.setKey(OverlayKey::sha1(Destination.getIp().get4().getInt()));
    Destination.setKey(globalNodeList->getNodeHandle(Destination.getIp())->getKey());
    Destination.setPort(globalNodeList->getNodeHandle(Destination.getIp())->getPort());

    if (category == NF_IPv4_ROUTE_ADDED || NF_IPv4_MROUTE_ADDED) {
        this->receiveNewNode(Destination, true, LOCAL, simTime());
    } else if (category == NF_IPv4_ROUTE_DELETED || NF_IPv4_MROUTE_DELETED) {
        //this->receiveNewNode(Destination, true, LOCAL, simTime());
    } else if (category == NF_IPv4_ROUTE_CHANGED || NF_IPv4_MROUTE_CHANGED) {
        this->receiveNewNode(Destination, true, LOCAL, simTime());
    }
}

}; //namespace
