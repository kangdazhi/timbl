/*
  Copyright (c) 1998 - 2016
  ILK   - Tilburg University
  CLST  - Radboud University
  CLiPS - University of Antwerp

  This file is part of timbl

  timbl is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 3 of the License, or
  (at your option) any later version.

  timbl is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, see <http://www.gnu.org/licenses/>.

  For questions and suggestions, see:
      https://github.com/LanguageMachines/timbl/issues
  or send mail to:
      lamasoftware (at ) science.ru.nl
*/
#include <exception>
#include <stdexcept>
#include <string>
#include <sstream>

#include "timbl/Common.h"
#include "timbl/MsgClass.h"
#include "timbl/Types.h"
#include "timbl/Instance.h"
#include "timbl/neighborSet.h"
#include "ticcutils/XMLtools.h"
#include "timbl/BestArray.h"

namespace Timbl {
  using namespace std;
  using namespace TiCC;
  using namespace Common;

  BestRec::BestRec():
    bestDistance( 0.0 )
  {}

  BestRec::~BestRec(){
    for ( auto const& b : bestDistributions ){
      delete b;
    }
  }

  BestArray::~BestArray(){
    for ( auto const& b : bestArray ){
      delete b;
    }
  }

  ostream& operator<< ( ostream& os, const BestRec *b ){
    if ( b ){
      os << b->aggregateDist.DistToString();
      int OldPrec = os.precision(DBL_DIG-1);
      os.setf(ios::showpoint);
      os << "\t" << b->bestDistance;
      os.precision(OldPrec);
      os << endl;
    }
    else {
      os << "bestrec is null!" << endl;
    }
    return os;
  }

  void BestArray::init( unsigned int numN, unsigned int maxB,
			bool storeI, bool showDi, bool showDb ){
    _storeInstances = storeI;
    _showDi = showDi;
    _showDb = showDb;
    maxBests = maxB;
    // When necessary, take a larger array. (initialy it has 0 length)
    // Also check if verbosity has changed and a BestInstances array
    // is required.
    //
    size_t S = size;
    size = numN;
    if ( S < size ){
      bestArray.reserve( size );
      for ( size_t k=S; k < size; ++k ) {
	bestArray.push_back( new BestRec() );
      }
    }
    size_t penalty = 0;
    for ( const auto& best : bestArray ){
      best->bestDistance = (DBL_MAX - numN) + penalty++;
      if ( best->bestInstances.empty() ){
	if ( _storeInstances ){
	  best->bestInstances.reserve( maxBests );
	  best->bestDistributions.reserve( maxBests );
	}
      }
      else {
	for ( auto const& bd : best->bestDistributions ){
	  delete bd;
	}
	best->bestInstances.clear();
	best->bestDistributions.clear();
      }
      best->aggregateDist.clear();
    }
  }

  double BestArray::addResult( double Distance,
			       const ValueDistribution *Distr,
			       const string& neighbor ){
    // We have the similarity in Distance, and a num_of_neighbors
    // dimensional array with best similarities.
    // Check, and add/replace/move/whatever.
    //
    for ( unsigned int k = 0; k < size; ++k ) {
      BestRec *best = bestArray[k];
      if (fabs(Distance - best->bestDistance) < Epsilon) {
	// Equal...just add to the end.
	//
	best->aggregateDist.Merge( *Distr );
	if ( _storeInstances && best->bestInstances.size() < maxBests ){
	  best->bestInstances.push_back( neighbor );
	  best->bestDistributions.push_back( Distr->to_VD_Copy() );
	}
	break;
      }
      // Check if better than bests[k], insert (or replace if
      // it's the lowest of the k bests).
      //
      /*
	Example (no_n = 3):
	k      distance      number
	0       2             3
	1       4             2
	2       6             1

	sim = 1 (dus beste)
      */
      else if (Distance < best->bestDistance) {
	if (k == size - 1) {
	  //
	  // Replace.
	  //
	  best->bestDistance = Distance;
	  if ( _storeInstances ){
	    for ( unsigned int j = 0; j < best->bestInstances.size(); ++j ){
	      delete best->bestDistributions[j];
	    }
	    best->bestInstances.clear();
	    best->bestDistributions.clear();
	    best->bestInstances.push_back( neighbor );
	    best->bestDistributions.push_back( Distr->to_VD_Copy() );
	  }
	  best->aggregateDist.clear();
	  best->aggregateDist.Merge( *Distr );
	}
	else {
	  //
	  // Insert. First shift the rest up.
	  //
	  BestRec *keep = bestArray[size-1];
	  for ( size_t i = size - 1; i > k; i--) {
	    bestArray[i] = bestArray[i-1];
	  } // i
	  //
	  // And now insert.
	  //
	  keep->bestDistance = Distance;
	  if ( _storeInstances ){
	    for ( unsigned int j = 0; j < keep->bestInstances.size(); ++j ){
	      delete keep->bestDistributions[j];
	    }
	    keep->bestInstances.clear();
	    keep->bestDistributions.clear();
	    keep->bestInstances.push_back( neighbor );
	    keep->bestDistributions.push_back( Distr->to_VD_Copy() );
	  }
	  keep->aggregateDist.clear();
	  keep->aggregateDist.Merge( *Distr );
	  bestArray[k] = keep;
	}
	break;
      } // Distance < fBest
    } // k
    return bestArray[size-1]->bestDistance;
  }

  void BestArray::initNeighborSet( neighborSet& ns ) const {
    ns.clear();
    for ( auto const& best : bestArray ){
      ns.push_back( best->bestDistance,
		    best->aggregateDist );
    }
  }

  void BestArray::addToNeighborSet( neighborSet& ns, size_t n ) const {
    ns.push_back( bestArray[n-1]->bestDistance,
		  bestArray[n-1]->aggregateDist );
  }

  xmlNode *BestArray::toXML() const {
    xmlNode *top = XmlNewNode( "neighborset" );
    size_t k = 0;
    for ( auto const& best : bestArray ){
      ++k;
      if ( _storeInstances ){
	size_t totalBests = best->totalBests();
	if ( totalBests == 0 )
	  break; // TRIBL algorithms do this!
	xmlNode *nbs = XmlNewChild( top, "neighbors" );
	XmlSetAttribute( nbs, "k", toString(k) );
	XmlSetAttribute( nbs, "total", toString(totalBests) );
	XmlSetAttribute( nbs, "distance", toString( best->bestDistance ) );
	if ( maxBests < totalBests )
	  XmlSetAttribute( nbs, "limited", toString( maxBests ) );
	for ( unsigned int m=0; m < best->bestInstances.size(); ++m ){
	  xmlNode *nb = XmlNewChild( nbs, "neighbor" );
	  XmlNewTextChild( nb, "instance", best->bestInstances[m] );
	  if ( _showDb )
	    XmlNewTextChild( nb, "distribution",
			     best->bestDistributions[m]->DistToString() );
	}
      }
      else {
	if ( best->aggregateDist.ZeroDist() )
	  break;
	xmlNode *nbs = XmlNewChild( top, "neighbors" );
	XmlSetAttribute( nbs, "k", toString(k) );
	if ( _showDb ){
	  XmlNewTextChild( nbs, "distribution",
			   best->aggregateDist.DistToString() );
	}
	if ( _showDi ){
	  XmlNewTextChild( nbs, "distance",
			   toString(best->bestDistance) );
	}
      }
    }
    return top;
  }

  ostream& operator<< ( ostream& os, const BestArray& bA ){
    size_t k = 0;
    for ( auto const& best : bA.bestArray ){
      ++k;
      if ( bA._storeInstances ){
	size_t totalBests = best->totalBests();
	if ( totalBests == 0 )
	  break; // TRIBL algorithms do this!
	os << "# k=" << k << ", " << totalBests
	   <<  " Neighbor(s) at distance: ";
	int OldPrec = os.precision(DBL_DIG-1);
	os.setf(ios::showpoint);
	os << "\t" << best->bestDistance;
	os.precision(OldPrec);
	if ( bA.maxBests <= best->bestInstances.size() )
	  os << " (only " << bA.maxBests << " shown)";
	os << endl;
	for ( unsigned int m=0; m < best->bestInstances.size(); ++m ){
	  os << "#\t" << best->bestInstances[m];
	  if ( bA._showDb )
	    os << best->bestDistributions[m]->DistToString() << endl;
	  else
	    os << " -*-" << endl;
	}
      }
      else {
	if ( best->aggregateDist.ZeroDist() )
	  break;
	os << "# k=" << k << "\t";
	if ( bA._showDb ){
	  os << best->aggregateDist.DistToString();
	}
	if ( bA._showDi ){
	  int OldPrec = os.precision(DBL_DIG-1);
	  os.setf(ios::showpoint);
	  os << best->bestDistance;
	  os.precision(OldPrec);
	}
	os << endl;
      }
    }
    return os;
  }

}
