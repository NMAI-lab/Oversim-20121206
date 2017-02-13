//
// Copyright (C) 2009 Institut fuer Telematik, Universitaet Karlsruhe (TH)
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
 * @file OneHopOverlay4MANET.h
 * @author Frank Ockenfeld
 * based on EpiChord.h, by Jamie Furness
 */

#ifndef _ONEHOPOVERLAY4MANET_
#define _ONEHOPOVERLAY4MANET_

#include <BaseOverlay.h>

#include "OneHopOverlay4MANETCache.h"
#include "OneHopOverlay4MANETMessage_m.h"
#include "IPv4Route.h"
#include "INotifiable.h"

namespace oversim {

class IInterfaceTable;
class NotificationBoard;
class OneHopOverlay4MANETNodeList;

/**
 * OneHopOverlay4MANET overlay module
 *
 * Implementation of the OneHopOverlay4MANET KBR overlay as described in
 * "OneHopOverlay4MANET: A one logicalhop overlay for MANETs and a single Chord Lookup Algorithm with Iterative Routing State Management" by Mohammad al Mojamed
 *
 * @author Frank Ockenfeld
 * based on EpiChord.h, by Jamie Furness
 */
class OneHopOverlay4MANET : public BaseOverlay
{
public:
	OneHopOverlay4MANET();
	virtual ~OneHopOverlay4MANET();


	// see BaseOverlay.h
	virtual void initializeOverlay(int stage);

	// see BaseOverlay.h
	virtual void handleTimerEvent(cMessage* msg);

	// see BaseOverlay.h
	virtual void recordOverlaySentStats(BaseOverlayMessage* msg);

	// see BaseOverlay.h
	virtual void finishOverlay();

	// see BaseOverlay.h
    OverlayKey distance(const OverlayKey& x, const OverlayKey& y, bool useAlternative = false) const;

	/**
	 * updates information shown in tk-environment
	 */
	virtual void updateTooltip();

protected:
	int successorListSize;
	double nodesPerSlice;
	int joinRetry;
	double joinDelay;
	double stabilizeDelay;
	bool stabilizeEstimation;
	double stabilizeEstimateMuliplier;
	double cacheFlushDelay;
	int cacheCheckMultiplier;
	int cacheCheckCounter;
	double cacheTTL;
	double nodeProbes;
	double nodeTimeouts;
	double cacheUpdateDelta;
	bool activePropagation;
	bool sendFalseNegWarnings;
	bool fibonacci;
	bool SuccAdded;
	bool PreAdded;

	// timer messages
	cMessage* join_timer;
	cMessage* stabilize_timer;
	cMessage* cache_timer;

	// statistics
	int joinCount;
	int joinBytesSent;
	int stabilizeCount;
	int stabilizeBytesSent;

	int keyLength;
    NodeHandle Destination;
    TransportAddress transpo;

	NodeVector* NodeVec = new NodeVector();

	// node references
	TransportAddress bootstrapNode;

	// module references
	OneHopOverlay4MANETNodeList* successorList;
	OneHopOverlay4MANETNodeList* predecessorList;
	OneHopOverlay4MANETCache* cache;

	// OneHopOverlay4MANET routines

    /**
     * callback-method for events at the NotificationBoard
     *
     * @param category cObject
     * @param details IPv4Address of destination node
     */
    virtual void receiveChangeNotification(int category, const cObject *details);

	/**
	 * Assigns the successor and predecessor list modules to our reference
	 */
	virtual void findFriendModules();

	/**
	 * initializes successor and predecessor lists
	 */
	virtual void initializeFriendModules();

	/**
	 * changes node state
	 *
	 * @param toState state to change to
	 */
	virtual void changeState(int toState);

	/**
	 * handle an expired join timer
	 *
	 * @param msg the timer self-message
	 */
	virtual void handleJoinTimerExpired(cMessage* msg);

	/**
	 * handle an expired stabilize timer
	 *
	 * @param msg the timer self-message
	 */
	virtual void handleStabilizeTimerExpired(cMessage* msg);

	/**
	 * handle an expired cache timer
	 *
	 * @param msg the timer self-message
	 */
	virtual void handleCacheFlushTimerExpired(cMessage* msg);

	// see BaseOverlay.h
	NodeVector* findNode(const OverlayKey& key, int numRedundantNodes, int numSiblings, BaseOverlayMessage* msg);

	// see BaseOverlay.h
	virtual void joinOverlay();

	// see BaseOverlay.h
	virtual bool isSiblingFor(const NodeHandle& node, const OverlayKey& key, int numSiblings, bool* err);

	// see BaseOverlay.h
	int getMaxNumSiblings();

	// see BaseOverlay.h
	int getMaxNumRedundantNodes();

	double calculateGamma();

	// see BaseOverlay.h
	virtual bool handleRpcCall(BaseCallMessage* msg);

	// see BaseOverlay.h
	virtual void handleRpcResponse(BaseResponseMessage* msg, cPolymorphic* context, int rpcId, simtime_t rtt);

	// see BaseOverlay.h
	virtual void handleRpcTimeout(BaseCallMessage* msg, const TransportAddress& dest, cPolymorphic* context, int rpcId, const OverlayKey& destKey);

	// see BaseOverlay.h
	virtual bool handleFailedNode(const TransportAddress& failed);

    //see BaseOverlay.h
    virtual void setOwnNodeID();

	void sendFalseNegWarning(NodeHandle bestPredecessor, NodeHandle bestSuccessor, NodeVector* deadNodes);

	virtual void rpcFalseNegWarning(OneHopOverlay4MANETFalseNegWarningCall* warning);

	AbstractLookup* createLookup(RoutingType routingType, const BaseOverlayMessage* msg, const cPacket* findNodeExt, bool appLookup);

	virtual void handleRpcFindNodeResponse(FindNodeResponse* response);

	virtual void receiveNewNode(const NodeHandle& node, bool direct, NodeSource source, simtime_t lastUpdate);

	friend class OneHopOverlay4MANETCache;
	friend class OneHopOverlay4MANETNodeList;
	friend class OneHopOverlay4MANETIterativeLookup;
	friend class OneHopOverlay4MANETIterativePathLookup;

private:
};

}; // namespace
#endif
