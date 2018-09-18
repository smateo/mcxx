/*--------------------------------------------------------------------
  (C) Copyright 2016-2016 Barcelona Supercomputing Center
                          Centro Nacional de Supercomputacion

  This file is part of Mercurium C/C++ source-to-source compiler.

  See AUTHORS file in the top level directory for information
  regarding developers and contributors.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 3 of the License, or (at your option) any later version.

  Mercurium C/C++ source-to-source compiler is distributed in the hope
  that it will be useful, but WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.  See the GNU Lesser General Public License for more
  details.

  You should have received a copy of the GNU Lesser General Public
  License along with Mercurium C/C++ source-to-source compiler; if
  not, write to the Free Software Foundation, Inc., 675 Mass Ave,
  Cambridge, MA 02139, USA.
--------------------------------------------------------------------*/


#ifndef TL_NANOS6_SUPPORT_HPP
#define TL_NANOS6_SUPPORT_HPP

#include "tl-objectlist.hpp"
#include "tl-scope.hpp"
#include "tl-nodecl.hpp"
#include "tl-nodecl-utils.hpp"

namespace TL
{
namespace Nanos6
{
    TL::Symbol get_nanos6_class_symbol(const std::string &name);
    TL::Symbol get_nanos6_function_symbol(const std::string &name);

    void add_extra_mappings_for_vla_types(
            TL::Type t,
            Scope sc,
            /* out */
            Nodecl::Utils::SimpleSymbolMap &symbol_map,
            TL::ObjectList<TL::Symbol> &vla_vars);
}
}

#endif // TL_NANOS6_SUPPORT_HPP
