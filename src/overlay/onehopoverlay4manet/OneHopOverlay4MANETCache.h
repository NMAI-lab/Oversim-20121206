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
 * @file OneHopOverlay4MANETCache.h
 * @author Frank Ockenfeld
 * based on EpiChordFingerCache.h, by Jamie Furness
 */

#ifndef __ONEHOPOVERLAY4MANETCache_H_
#define __ONEHOPOVERLAY4MANETCache_H_

#include <map>

#include <omnetpp.h>
#include <GlobalNodeList.h>
#include <NodeVector.h>
#include <InitStages.h>

class BaseOverlay;

namespace oversim {

enum NodeSource { OBSERVED, LOCAL, MAINTENANCE, CACHE_TRANSFER };

struct OneHopOverlay4MANETCacheEntry
{
	NodeHandle nodeHandle;
	simtime_t lastUpdate;
	simtime_t added;
	double ttl;
	NodeSource source;
};

typedef std::map<OverlayKey, OneHopOverlay4MANETCacheEntry> CacheMap;
typedef std::map<OverlayKey, OneHopOverlay4MANETCacheEntry> DeadMap;

class OneHopOverlay4MANET;

std::ostream& operator<<(std::ostream& os, const OneHopOverlay4MANETCacheEntry& e);

/**
 * OneHopOverlay4MANET's cache module
 *
 * This module contains the Cache of the OneHopOverlay4MANET implementation.
 *
 * @author Frank Ockenfeld
 * based on EpiChordFingerCache.h, by Jamie Furness
 */
class OneHopOverlay4MANETCache : public cSimpleModule
{
public:

	virtual int numInitStages() const
	{
		return MAX_STAGE_OVERLAY + 1;
	}

	virtual void initialize(int stage);
	virtual void handleMessage(cMessage* msg);

	/**
	 * Sets up the Cache
	 *
	 * Sets up the Cache and makes all Entries as empty.
	 * Should be called on startup to initialize the Cache.
	 *
	 * @param overlay pointer to the main chord module
	 */
	virtual void initializeCache(NodeHandle owner, OneHopOverlay4MANET* overlay, double ttl);

	virtual void updateEntry(const NodeHandle& node, bool direct, NodeSource source, simtime_t lastUpdate = simTime()) { updateEntry(node, direct, lastUpdate, ttl, source); }
	virtual void updateEntry(const NodeHandle& node, bool direct, simtime_t lastUpdate, double ttl, NodeSource source);

	virtual void setEntryTTL(const NodeHandle& node) { setEntryTTL(node, ttl); }
	virtual void setEntryTTL(const NodeHandle& node, double ttl);

	bool handleFailedNode(const TransportAddress& failed);

	void removeOldEntries();

	OneHopOverlay4MANETCacheEntry* getNode(const NodeHandle& node);
	OneHopOverlay4MANETCacheEntry* getNode(uint32_t pos);
	std::vector<OneHopOverlay4MANETCacheEntry> getDeadRange(OverlayKey start, OverlayKey end);

	uint32_t countSlice(OverlayKey startOffset, OverlayKey endOffset);
	bool isDead(const NodeHandle& node);

	virtual uint32_t getSize();
	virtual uint32_t countLive();
	virtual uint32_t countRealLive();
	virtual uint32_t countDead();
	virtual uint32_t countRealDead();

	virtual int getSuccessfulUpdates() { return successfulUpdates; }

	void findBestHops(OverlayKey key, NodeVector* nodes, std::vector<simtime_t>* lastUpdates, std::set<NodeHandle>* exclude, int numRedundantNodes);

	simtime_t estimateNodeLifetime(int minSampleSize = 5);

	virtual bool contains(const TransportAddress& node);
	virtual void display();

protected:
	CacheMap liveCache;
	DeadMap deadCache;
	NodeHandle thisNode;
	OneHopOverlay4MANET* overlay;
	double ttl;
	int successfulUpdates;
	GlobalNodeList* globalNodeList;
};

}; // namespace

#endif
