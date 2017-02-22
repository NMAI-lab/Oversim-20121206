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
 * @file OneHopOverlay4MANETNodeList.cc
 * @author Frank Ockenfeld
 * based on EpiChordNodeList.cc, by Jamie Furness
 */

#include <cassert>

#include "OneHopOverlay4MANETCache.h"
#include "OneHopOverlay4MANETNodeList.h"
#include "OneHopOverlay4MANET.h"

namespace oversim {

Define_Module(OneHopOverlay4MANETNodeList);

std::ostream& operator<<(std::ostream& os, const OneHopOverlay4MANETNodeListEntry& e)
{
	os << e.nodeHandle << " " << e.newEntry;
	return os;
};

void OneHopOverlay4MANETNodeList::initialize(int stage)
{
	// because of IPAddressResolver, we need to wait until interfaces
	// are registered, address auto-assignment takes place etc.
	if (stage != MIN_STAGE_OVERLAY)
		return;

	WATCH_MAP(nodeMap);
}

void OneHopOverlay4MANETNodeList::handleMessage(cMessage* msg)
{
	throw new cRuntimeError("this module doesn't handle messages, it runs only in initialize()");
}

void OneHopOverlay4MANETNodeList::initializeList(uint32_t size, NodeHandle owner, OneHopOverlay4MANETCache* cache, OneHopOverlay4MANET *overlay, bool forwards)
{
	nodeMap.clear();
	nodeListSize = size;
	thisNode = owner;
	this->cache = cache;
	this->overlay = overlay;
	this->forwards = forwards;

	//addNode(thisNode);

	addNode2(thisNode, simTime(), false);
}

uint32_t OneHopOverlay4MANETNodeList::getSize()
{
	return nodeMap.size();
}

bool OneHopOverlay4MANETNodeList::isEmpty()
{
	return nodeMap.size() == 1 && getNode() == thisNode;
}

bool OneHopOverlay4MANETNodeList::isFull()
{
	if (nodeMap.size() == 0)
		return false;

	NodeMap::iterator it = nodeMap.end();
	it--;

	return it->second.nodeHandle != thisNode;
}

const simtime_t& OneHopOverlay4MANETNodeList::getTimestamp(uint32_t pos)
{
    // check boundaries
    if (pos == 0 && nodeMap.size() == 0)
        return simtime_t::SCALEEXP_UNINITIALIZED;

    if (pos >= nodeMap.size())
        throw cRuntimeError("Index out of bound (OneHopOverlay4MANETNodeList, getNode())");

    NodeMap::iterator it = nodeMap.begin();

    for (uint32_t i = 0; i < pos; i++) {
        it++;
        if (i == (pos - 1))
            return it->second.lastUpdate;
    }
    return it->second.lastUpdate;
}

const OneHopOverlay4MANETNodeListEntry& OneHopOverlay4MANETNodeList::getEntry(uint32_t pos)
{
    // check boundaries
    if (pos == 0 && nodeMap.size() == 0)
        ;

    if (pos >= nodeMap.size())
        throw cRuntimeError("Index out of bound (OneHopOverlay4MANETNodeList, getNode())");

    NodeMap::iterator it = nodeMap.begin();

    for (uint32_t i = 0; i < pos; i++) {
        it++;
        if (i == (pos - 1))
            return it->second;
    }
    return it->second;
}

const NodeHandle& OneHopOverlay4MANETNodeList::getNode(uint32_t pos)
{
	// check boundaries
	if (pos == 0 && nodeMap.size() == 0)
		return NodeHandle::UNSPECIFIED_NODE;

	if (pos >= nodeMap.size())
		throw cRuntimeError("Index out of bound (OneHopOverlay4MANETNodeList, getNode())");

	NodeMap::iterator it = nodeMap.begin();

	for (uint32_t i = 0; i < pos; i++) {
		it++;
		if (i == (pos - 1))
			return it->second.nodeHandle;
	}
	return it->second.nodeHandle;
}

void OneHopOverlay4MANETNodeList::findBestHop(OverlayKey key, NodeVector* nodes, std::vector<simtime_t>* lastUpdates, std::set<NodeHandle>* exclude, int numRedundantNodes)
{
    key -= thisNode.getKey() + OverlayKey::ONE;

    // Remove any old entries from the cache so we don't return any expired entries
    //removeOldEntries;

    // locate the node we want
    NodeMap::iterator it = nodeMap.lower_bound(key);
    if (it == nodeMap.end()) // This shouldn't happen!
        it = nodeMap.begin();

    // Store the first node so we can detect loops
    NodeHandle* first = &it->second.nodeHandle;

    // Keep going forwards until we find an alive node
    while (exclude->find(it->second.nodeHandle) != exclude->end()) {
        it++;

        if (it == nodeMap.end())
            it = nodeMap.begin();

        // We have already tried this node so we must have looped right around and not found a single alive node
        if (&it->second.nodeHandle == first)
            return;
    }

    first = &it->second.nodeHandle;

    for (int i = 0;i < numRedundantNodes;) {
        // Add the node
        if (exclude->find(it->second.nodeHandle) == exclude->end()) {
            nodes->push_back(it->second.nodeHandle);
            lastUpdates->push_back(it->second.lastUpdate);
            i++;
        }

        // Check the predecessor iterator hasn't gone past the start
        if (it == nodeMap.begin())
            it = nodeMap.end();

        it--;

        // We have already tried this node so we must have looped right around
        if (&it->second.nodeHandle == first)
            break;
    }
}

void OneHopOverlay4MANETNodeList::updateNode(NodeHandle node, simtime_t lastUpdate)
{
    if (node.isUnspecified())
        return;

    OneHopOverlay4MANETNodeListEntry entry;
    entry.nodeHandle = node;
    entry.lastUpdate = lastUpdate;
    entry.newEntry = false;

    //replace entry
    NodeMap::iterator it = nodeMap.find(node.getKey());
    it->second.lastUpdate = lastUpdate;
    nodeMap.find(node.getKey()) = it;
}

void OneHopOverlay4MANETNodeList::addNode2(NodeHandle node, simtime_t lastUpdate, bool resize)
{
    if (node.isUnspecified() || cache->isDead(node))
        return;

    bool changed = false;
    OverlayKey sum = node.getKey() - thisNode.getKey();

    // If sorting backwards we need to invert the offset
    if (!forwards)
        sum = OverlayKey::ZERO - sum;

    sum -= OverlayKey::ONE;

    NodeMap::iterator it = nodeMap.find(sum);

    // Make a CommonAPI update() upcall to inform application
    // about our new neighbor in the node list

    if (it == nodeMap.end()) {
        changed = true;

        OneHopOverlay4MANETNodeListEntry entry;
        entry.nodeHandle = node;
        entry.lastUpdate = lastUpdate;
        entry.newEntry = true;

        nodeMap[sum] = entry;

        overlay->callUpdate(node, true);
    }
    else
        it->second.newEntry = true;

    if (node != thisNode) {
        cache->updateEntry(node, true, simTime(), 0, LOCAL);
        assert (cache->getNode(node) != NULL);
    }

    if ((resize == true) && (nodeMap.size() > (uint32_t)nodeListSize)) {
        it = nodeMap.end();
        it--;

        // If we simply removed the new node again
        if (node == it->second.nodeHandle)
            changed = false;

        overlay->callUpdate(it->second.nodeHandle, false);
        cache->setEntryTTL(it->second.nodeHandle);
        nodeMap.erase(it);
    }

    if (changed && node != thisNode)
        additions.push_back(node);
    }

void OneHopOverlay4MANETNodeList::addNode(NodeHandle node, bool resize)
{

}

bool OneHopOverlay4MANETNodeList::contains(const TransportAddress& node)
{
	for (NodeMap::iterator it = nodeMap.begin();it != nodeMap.end();it++)
		if (node == it->second.nodeHandle)
			return true;

	return false;
}

bool OneHopOverlay4MANETNodeList::handleFailedNode(const TransportAddress& failed)
{
	assert(failed != thisNode);
	for (NodeMap::iterator it = nodeMap.begin();it != nodeMap.end();it++) {
		if (failed == it->second.nodeHandle) {
			nodeMap.erase(it);
			overlay->callUpdate(failed, false);

			// ensure that thisNode is always in the node list
			if (getSize() == 0)
				addNode(thisNode);

			return true;
		}
	}
	return false;
}

void OneHopOverlay4MANETNodeList::removeOldEntries(simtime_t currentTime, double cacheTTL)
{
    //assert(!isEmpty());

    NodeMap::iterator it;
    simtime_t minTime;

    minTime.operator =(currentTime);
    minTime.operator -=(cacheTTL);
    for (it = nodeMap.begin(); it != nodeMap.end();it++) {
        if(it->second.lastUpdate.operator <=(minTime) == true){
            overlay->callUpdate(it->second.nodeHandle, false);
            cache->setEntryTTL(it->second.nodeHandle);
            nodeMap.erase(it++);
        }
        else {
            it->second.newEntry = false;
            it++;
        }
    }
}

//got replaced with removeOldEntries
void OneHopOverlay4MANETNodeList::removeOldNodes()
{
	/*NodeMap::iterator it;

	for (it = nodeMap.begin(); it != nodeMap.end();) {
		if (it->second.newEntry == false) {
			overlay->callUpdate(it->second.nodeHandle, false);
			cache->setEntryTTL(it->second.nodeHandle);
			nodeMap.erase(it++);
		}
		else {
			it->second.newEntry = false;
			it++;
		}
	}

	it = nodeMap.end();
	it--;

	while (nodeMap.size() > nodeListSize) {
		cache->setEntryTTL(it->second.nodeHandle);
		nodeMap.erase(it--);
	}

	if (getSize() == 0)
		addNode(thisNode);

	assert(!isEmpty());*/
}

bool OneHopOverlay4MANETNodeList::hasChanged()
{
	return !additions.isEmpty();
}

NodeVector* OneHopOverlay4MANETNodeList::getAdditions()
{
	return &additions;
}

void OneHopOverlay4MANETNodeList::updateDisplayString()
{
	// FIXME: doesn't work without tcl/tk
	//		if (ev.isGUI()) {
	if (1) {
		char buf[80];

		if (nodeMap.size() == 1)
			sprintf(buf, "1 node");
		else
			sprintf(buf, "%zi nodes", nodeMap.size());

		getDisplayString().setTagArg("t", 0, buf);
		getDisplayString().setTagArg("t", 2, "blue");
	}

}

void OneHopOverlay4MANETNodeList::updateTooltip()
{
	if (ev.isGUI()) {
        std::stringstream str;
		for (uint32_t i = 0; i < nodeMap.size(); i++)	{
			str << getNode(i);
			if (i != nodeMap.size() - 1)
				str << endl;
		}

		char buf[1024];
		sprintf(buf, "%s", str.str().c_str());
		getDisplayString().setTagArg("tt", 0, buf);
	}
}

void OneHopOverlay4MANETNodeList::display()
{
	std::cout << "Content of OneHopOverlay4MANETNodeList:" << endl;
	for (NodeMap::iterator it = nodeMap.begin(); it != nodeMap.end(); it++)
		std::cout << it->first << " with Node: " << it->second.nodeHandle << endl;
}

}; //namespace
