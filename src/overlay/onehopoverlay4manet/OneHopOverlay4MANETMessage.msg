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

//
// author Frank Ockenfeld
// based on EpiChordMessage.msg, by Jamie Furness
//

cplusplus {{
#include <NodeHandle.h>
#include <TransportAddress.h>
#include <CommonMessages_m.h>

static const int ONEHOPOVERLAY4MANETCOMMAND_L = 8;
static const int ONEHOPOVERLAY4MANETNODETYPE_L = 8;
static const int ONEHOPOVERLAY4MANETTTL_L = 8;

#define ONEHOPOVERLAY4MANET_JOINCALL_L(msg) BASECALL_L(msg)
#define ONEHOPOVERLAY4MANET_JOINRESPONSE_L(msg) (BASERESPONSE_L(msg) + (msg->getSucNodeArraySize() * NODEHANDLE_L) + (msg->getPreNodeArraySize() * NODEHANDLE_L) + (msg->getCacheNodeArraySize() * NODEHANDLE_L) + (msg->getCacheLastUpdateArraySize() * ONEHOPOVERLAY4MANETTTL_L))
#define ONEHOPOVERLAY4MANET_JOINACKCALL_L(msg) BASECALL_L(msg)
#define ONEHOPOVERLAY4MANET_JOINACKRESPONSE_L(msg) BASERESPONSE_L(msg)

#define ONEHOPOVERLAY4MANET_FALSENEGWARNINGCALL_L(msg) (BASECALL_L(msg) + NODEHANDLE_L + (msg->getDeadNodeArraySize() * NODEHANDLE_L))
#define ONEHOPOVERLAY4MANET_FALSENEGWARNINGRESPONSE_L(msg) BASERESPONSE_L(msg)

#define ONEHOPOVERLAY4MANET_STABILIZECALL_L(msg) (BASECALL_L(msg) + (msg->getAdditionsArraySize() * NODEHANDLE_L) + ONEHOPOVERLAY4MANETNODETYPE_L)
#define ONEHOPOVERLAY4MANET_STABILIZERESPONSE_L(msg) (BASERESPONSE_L(msg) + (msg->getPredecessorsArraySize() * NODEHANDLE_L) + (msg->getSuccessorsArraySize() * NODEHANDLE_L) + (msg->getDeadArraySize() * NODEHANDLE_L) + ONEHOPOVERLAY4MANETNODETYPE_L)

#define ONEHOPOVERLAY4MANET_FINDNODEEXTMESSAGE_L(msg) ((msg->getLastUpdatesArraySize() * ONEHOPOVERLAY4MANETTTL_L))
#define ONEHOPOVERLAY4MANETCOORD_BROADCASTINFO_L(msg) (KEY_L)
}}

class noncobject NodeHandle;
class noncobject TransportAddress;
class noncobject OverlayKey;

class BaseOverlayMessage;
class BaseCallMessage;
class BaseResponseMessage;

class noncobject OneHopOverlay4MANETNodeType;

enum OneHopOverlay4MANETNodeType
{
	PREDECESSOR = 0;
	SUCCESSOR = 1;
}

//
// Message used to join an OneHopOverlay4MANET overlay
//
packet OneHopOverlay4MANETJoinCall extends BaseCallMessage
{
}

//
// Message used to answer to a JoinCall message
//
packet OneHopOverlay4MANETJoinResponse extends BaseResponseMessage
{
	NodeHandle sucNode[];		// successor list of the message originator
	NodeHandle preNode[];		// predecessor list of the message originator

	NodeHandle cacheNode[];		// nodes in the cache table
	simtime_t cacheLastUpdate[];	// expire times of nodes in the cache table
}

packet OneHopOverlay4MANETJoinAckCall extends BaseCallMessage
{
}

packet OneHopOverlay4MANETJoinAckResponse extends BaseResponseMessage
{
}

packet OneHopOverlay4MANETFalseNegWarningCall extends BaseCallMessage
{
	NodeHandle bestPredecessor;
	NodeHandle deadNode[];
}

packet OneHopOverlay4MANETFalseNegWarningResponse extends BaseResponseMessage
{
}

//
// Message used for the  stabilization protocol
//
packet OneHopOverlay4MANETStabilizeCall extends BaseCallMessage
{
	NodeHandle additions[];
	int nodeType enum(OneHopOverlay4MANETNodeType);	// Type of node we are
}

//
// Message used to answer to a StabilizeCall message
//
packet OneHopOverlay4MANETStabilizeResponse extends BaseResponseMessage
{
	NodeHandle predecessors[];		// predecessor list of the message originator
	simtime_t predecessorsLastUpdate[];
	NodeHandle successors[];		// successor list of the message originator
	simtime_t successorsLastUpdate[];
	NodeHandle dead[];				// dead nodes within the message originators neighbour list
}

//
// Message used to find nodes in the OneHopOverlay4MANET overlay
//
packet OneHopOverlay4MANETFindNodeExtMessage
{
	simtime_t lastUpdates[];		// expire times of the nodes in this findNodeResponse
}

packet OneHopOverlay4MANETCoordBroadcastInfo
{
	OverlayKey limit;
}
