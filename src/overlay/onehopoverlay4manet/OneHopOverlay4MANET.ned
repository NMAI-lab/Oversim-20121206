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

package oversim.overlay.onehopoverlay4manet;

import oversim.common.BaseOverlay;
import oversim.common.IOverlay;


@namespace(oversim);

//
// The main module of the OneHopOverlay4MANET implementation
//
// author Frank Ockenfeld
// based on EpiChord.ned, by Jamie Furness
//
simple OneHopOverlay4MANET extends BaseOverlay
{
    parameters:
        @class(OneHopOverlay4MANET);
        int successorListSize; // max number of successors in the SuccessorList
        int cacheSize;
        int nodesPerSlice; // number of nodes that should be in a slice (j)
        // after joinRetry unsuccessful join requests, we fetch
        // a need bootstrap node from the GlobalNodeList
        int joinRetry;
        double joinDelay @unit(s); // delay between join retries (sec)
        bool stabilizeEstimation; // should we estimate the stabilize interval or use stabilizeDelay?
        double stabilizeEstimateMuliplier;
        double stabilizeDelay @unit(s); // delay between stabilization attempts (sec)
        double cacheFlushDelay @unit(s); // delay between finger cache flushes (sec)
        int cacheCheckMultiplier; // how many cache flushes to perform before checking the invariant
        double cacheTTL @unit(s); // how long nodes should stay in the cache for (sec)
        double cacheUpdateDelta; // weighting to apply to old cache freshness ratios
        bool activePropagation; // should we call stabilization routines as soon as our neighbour lists are changed?
        bool sendFalseNegWarnings; // should we send warnings to fix false negatives?
        bool fibonacci; // use regular (logarithmic) finger spacing, or fibonacci finger spacing as with F-Chord
}

//
// This module contains the node list of the OneHopOverlay4MANET implementation.
//
// author Frank Ockenfeld
//
simple OneHopOverlay4MANETNodeList
{
    parameters:
        @display("i=block/table");
}

//
// This module contains the finger table of the OneHopOverlay4MANET implementation.
//
// author Frank Ockenfeld
//
simple OneHopOverlay4MANETCache
{
    parameters:
        @display("i=block/table");
}

//
// Implementation of the OneHopOverlay4MANET KBR overlay as described in
// "OneHopOverlay4MANET: A one logicalhop overlay for MANETs and a single Chord Lookup Algorithm with Iterative Routing State Management" by Mohammad al Mojamed
//
// author Frank Ockenfeld
//
module OneHopOverlay4MANETModules like IOverlay
{
    parameters:
        @display("i=block/network2;bgb=399,140");

    gates:
        input udpIn;   // gate from the UDP layer
        output udpOut;    // gate to the UDP layer
        input tcpIn;    // gate from the TCP layer
        output tcpOut;    // gate to the TCP layer
        input appIn;   // gate from the application
        output appOut;    // gate to the application

    submodules:
        onehopoverlay4manet: OneHopOverlay4MANET {
            parameters:
                @display("p=60,60");
        }
        Cache: OneHopOverlay4MANETCache {
            parameters:
                @display("p=158,60");
        }
        successorList: OneHopOverlay4MANETNodeList {
            parameters:
                @display("p=240,60");
        }
        predecessorList: OneHopOverlay4MANETNodeList {
            parameters:
                @display("p=332,60");
        }
    connections allowunconnected:
        udpIn --> onehopoverlay4manet.udpIn;
        udpOut <-- onehopoverlay4manet.udpOut;
        appIn --> onehopoverlay4manet.appIn;
        appOut <-- onehopoverlay4manet.appOut;
}
