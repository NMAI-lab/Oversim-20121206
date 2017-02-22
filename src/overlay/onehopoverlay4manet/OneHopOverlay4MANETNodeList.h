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
 * @file OneHopOverlay4MANETNodeList.h
 * @author Frank Ockenfeld
 * based on EpiChordNodeList.h, by Jamie Furness
 */

#ifndef __ONEHOPOVERLAY4MANETNODELIST_H_
#define __ONEHOPOVERLAY4MANETNODELIST_H_

#include <map>

#include <omnetpp.h>
#include <InitStages.h>
#include <NodeHandle.h>

class OverlayKey;
class OneHopOverlay4MANETStabilizeResponse;

namespace oversim {

struct OneHopOverlay4MANETNodeListEntry
{
	NodeHandle nodeHandle ;//*< the nodehandle
	simtime_t lastUpdate;
	bool newEntry;  //*< true, if this entry has just been added
};

typedef std::map<OverlayKey, OneHopOverlay4MANETNodeListEntry> NodeMap;

class OneHopOverlay4MANETCache;
class OneHopOverlay4MANET;

std::ostream& operator<<(std::ostream& os, const OneHopOverlay4MANETNodeListEntry& e);


/**
 * OneHopOverlay4MANET's node list module
 *
 * This module contains the node list of the OneHopOverlay4MANET implementation.
 *
 * @author Frank Ockenfeld, Jamie Furness, Markus Mauch, Ingmar Baumgart
 * @see OneHopOverlay4MANET
 */
class OneHopOverlay4MANETNodeList : public cSimpleModule
{
  public:
	virtual int numInitStages() const
	{
		return MAX_STAGE_OVERLAY + 1;
	}
	virtual void initialize(int stage);
	virtual void handleMessage(cMessage* msg);

	/**
	 * Initializes the node list. This should be called on startup
	 *
	 * @param size maximum number of neighbors in the node list
	 * @param owner the node owner is added to the node list
	 * @param overlay pointer to the main chord module
	 * @param forwards the direction in which to access nodes
	 */
	virtual void initializeList(uint32_t size, NodeHandle owner, OneHopOverlay4MANETCache* cache, OneHopOverlay4MANET* overlay, bool forwards);

	/**
	 * Returns number of neighbors in the node list
	 *
	 * @return number of neighbors
	 */
	virtual uint32_t getSize();

	/**
	 * Checks if the node list is empty
	 *
	 * @return returns false if the node list contains other nodes
	 *		 than this node, true otherwise.
	 */
	virtual bool isEmpty();

	/**
	 * Checks if the node list is full.
	 * In other words, does it contain outself still
	 *
	 * @return returns false if the node list doesn't contain this node,
	 * 		true otherwise.
	 */
	virtual bool isFull();

    /**
     * Returns a particular node
     *
     * @param pos position in the node list
     * @return timestamp of node at position pos
     */
    virtual const simtime_t& getTimestamp(uint32_t pos = 0);

	/**
	 * Returns a particular node
	 *
	 * @param pos position in the node list
	 * @return node at position pos
	 */
	virtual const NodeHandle& getNode(uint32_t pos = 0);

	virtual const OneHopOverlay4MANETNodeListEntry& getEntry(uint32_t pos = 0);

	/**
	 * Adds new nodes to the node list
	 *
	 * Adds new nodes to the node list and sorts the
	 * list using the corresponding keys. If the list size exceeds
	 * the maximum size nodes at the end of the list will be removed.
	 *
	 * @param node the node handle of the node to be added
	 * @param lastUpdate the simulation time when node was received
	 * @param resize if true, shrink the list to nodeListSize
	 */
	virtual void addNode2(NodeHandle node, simtime_t lastUpdate, bool resize);

	virtual void addNode(NodeHandle node, bool resize = true);

    /**
     * Updates timestamp of nodes
     *
     * Searches for the node key within NodeMap and updates the timestamp
     *
     * @param node the node handle that's to be updated
     * @param lastUpdate the new replacement simulation time
     */
	virtual void updateNode(NodeHandle node, simtime_t lastUpdate);

	virtual bool contains(const TransportAddress& node);

	void findBestHop(OverlayKey key, NodeVector* nodes, std::vector<simtime_t>* lastUpdates, std::set<NodeHandle>* exclude, int numRedundantNodes);

	bool handleFailedNode(const TransportAddress& failed);

	void removeOldEntries(simtime_t currentTime, double cacheTTL);

	void removeOldNodes();

	bool hasChanged();
	NodeVector* getAdditions();

	void display ();


  protected:
	NodeHandle thisNode; /**< own node handle */
	NodeMap nodeMap; /**< internal representation of the node list */

	NodeVector additions;

	uint32_t nodeListSize; /**< maximum size of the node list */
	bool forwards;

	OneHopOverlay4MANETCache* cache; /**< pointer to OneHopOverlay4MANETs cache module */
	OneHopOverlay4MANET* overlay; /**< pointer to the main OneHopOverlay4MANET module */

	/**
	 * Displays the current number of nodes in the list
	 */
	void updateDisplayString();

	/**
	 * Displays the first 4 nodes as tooltip.
	 */
	void updateTooltip();
};

}; //namespace
#endif
