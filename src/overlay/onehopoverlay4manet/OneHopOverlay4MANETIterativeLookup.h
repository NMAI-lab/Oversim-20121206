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
 * @file OneHopOverlay4MANETIterativeLookup.h
 * @author Frank Ockenfeld
 * based on EpiChordIterativeLookup.h, by Jamie Furness
 */

#ifndef __ONEHOPOVERLAY4MANETITERATIVE_LOOKUP_H
#define __ONEHOPOVERLAY4MANETITERATIVE_LOOKUP_H

#include <IterativeLookup.h>

#include "OneHopOverlay4MANET.h"
#include "OneHopOverlay4MANETNodeList.h"

namespace oversim {

class OneHopOverlay4MANETIterativeLookup : public IterativeLookup
{
	friend class OneHopOverlay4MANETIterativePathLookup;

protected:
	OneHopOverlay4MANET* onehopoverlay4manet;

public:
	OneHopOverlay4MANETIterativeLookup(BaseOverlay* overlay, RoutingType routingType, const IterativeLookupConfiguration& config, const cPacket* findNodeExt = NULL, bool appLookup = false);

	IterativePathLookup* createPathLookup();
};

class OneHopOverlay4MANETIterativePathLookup : public IterativePathLookup
{
	friend class OneHopOverlay4MANETIterativeLookup;

protected:
	OneHopOverlay4MANET* onehopoverlay4manet;

	NodeHandle bestPredecessor;
	NodeHandle bestPredecessorsSuccessor;
	NodeHandle bestSuccessor;
	NodeHandle bestSuccessorsPredecessor;

	OneHopOverlay4MANETIterativePathLookup(OneHopOverlay4MANETIterativeLookup* lookup, OneHopOverlay4MANET* onehopoverlay4manet);

	void handleResponse(FindNodeResponse* msg);
	void handleTimeout(BaseCallMessage* msg, const TransportAddress& dest, int rpcId);

	void checkFalseNegative();

	LookupEntry* getPreceedingEntry(bool incDead = false, bool incUsed = true);
	LookupEntry* getSucceedingEntry(bool incDead = false, bool incUsed = true);
	LookupEntry* getNextEntry();
};

}; //namespace

#endif
