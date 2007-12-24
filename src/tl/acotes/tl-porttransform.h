/*
    Acotes Translation Phase
    Copyright (C) 2007 - David Rodenas Pico <david.rodenas@bsc.es>
    Barcelona Supercomputing Center - Centro Nacional de Supercomputacion
    Universitat Politecnica de Catalunya

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
    
    $Id: tl-acotestransform.cpp 1611 2007-07-10 09:28:44Z drodenas $
*/
// 
// File:   tl-porttransform.h
// Author: drodenas
//
// Created on 22 / desembre / 2007, 19:11
//

#ifndef _TL_PORTTRANSFORM_H
#define	_TL_PORTTRANSFORM_H

#include <assert.h>
#include <string>

namespace TL { namespace Acotes {
    
    class Port;
    
    class PortTransform {
    
    // -- Auxiliary generators
    public:
        static std::string generatePort(Port* port);
        static std::string generateAcquire(Port* port);
        static std::string generatePeek(Port* port);
        static std::string generatePop(Port* port);
        static std::string generatePush(Port* port);
    private:
        static std::string generateInputPort(Port* port);
        static std::string generateVirtualInputPort(Port* port);
        static std::string generateOutputPort(Port* port);
        static std::string generateVirtualOutputPort(Port* port);
                
    // -- No constructor    
    private:
        PortTransform();
        
    };
    
} /* end namespace Acotes */ } /* end namespace TL */

#endif	/* _TL_PORTTRANSFORM_H */

