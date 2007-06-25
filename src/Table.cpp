/*
    SLB - Simple Lua Binder
    Copyright (C) 2007 Jose L. Hidalgo Valiño (PpluX)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

	Jose L. Hidalgo (www.pplux.com)
	pplux@pplux.com
*/

#include <SLB/Table.hpp>
#include <SLB/lua.hpp>
#include <SLB/Debug.hpp>

namespace SLB {

	Table::Table(const std::string &sep, bool c) : _cacheable(c), _separator(sep) {}
	Table::~Table() {}
		
	Object* Table::rawGet(const std::string &name)
	{
		Elements::iterator i = _elements.find(name);
		if (i == _elements.end())
		{
			SLB_DEBUG(10, "Access Table(%p) [%s] (FAILED!)", this, name.c_str());
			return 0;
		}
		SLB_DEBUG(10, "Access Table(%p) [%s] (OK!)", this, name.c_str());
		return i->second.get();
	}

	inline void Table::rawSet(const std::string &name, Object *obj)
	{
		if (obj == 0)
		{
			SLB_DEBUG(6, "Table (%p) remove '%s'", this, name.c_str());
			_elements.erase(name);
		}
		else
		{
			SLB_DEBUG(6, "Table (%p) [%s] = %p", this, name.c_str(), obj);
			_elements[name] = obj;		
		}
	}

	Object* Table::get(const std::string &name)
	{
		TableFind t = getTable(name, false);
		if (t.first != 0) return t.first->rawGet(t.second);
		return 0;
	}

	void Table::set(const std::string &name, Object *obj)
	{
		TableFind t = getTable(name, true);
		t.first->rawSet(t.second, obj);
	}

	Table::TableFind Table::getTable(const std::string &key, bool create)
	{
		if (_separator.empty()) return TableFind(this,key);

		std::string::size_type pos = key.find(_separator);
		if (pos != std::string::npos)
		{
			const std::string &base = key.substr(0, pos);
			const std::string &next = key.substr(pos+_separator.length());

			Table* subtable = dynamic_cast<Table*>(rawGet(base));
			
			if (subtable == 0)
			{
				if (create)
				{
					SLB_DEBUG(6, "Table (%p) create Subtable %s -> %s", this, 
						base.c_str(), next.c_str());

					subtable = new Table(_separator, _cacheable);
					rawSet(base,subtable);
				}
				else
				{
					return TableFind(0,key); // not found
				}
			}

			return subtable->getTable(next, create); //< TODO: recursivity... replace with iterative version.
		}
		// else :
		return TableFind(this,key);
	}
	
	int Table::get(lua_State *L, const std::string &key)
	{
		Object *obj = get(key);
		if (obj)
		{
			obj->push(L);
		}
		else
		{
			lua_pushnil(L);
		}
		return 1;
	}

	int Table::get(lua_State *L, long int)
	{
		lua_pushnil(L);
		return 1;
	}

	int Table::__index(lua_State *L)
	{
		int result = -1;
		luaL_checktype(L, 1, LUA_TTABLE);

		if (lua_type(L, 2) == LUA_TSTRING)
		{
			result = get(L, std::string(lua_tostring(L,2)));
		}
		else if (lua_type(L, 2) == LUA_TNUMBER)
		{
			result = get(L, (long int) lua_tointeger(L,2));
		}

		if (result >= 0) // result should be >= 0
		{
			if ( _cacheable && result == 1 )
			{
				SLB_DEBUG(10, "L(%p) table(%p) key %s (->to cache)", L, this, lua_tostring(L,2));
				lua_pushvalue(L,2); // key
				lua_pushvalue(L,-2); // returned value
				lua_rawset(L, 1); // table
			}
			return result;
		}
		
		// result -1
		luaL_error(L, "Table (%p) can not use %s as key", (void*)this, lua_typename(L, lua_type(L,2)));
		return result;
	}

	int Table::__newindex(lua_State *L)
	{
		luaL_error(L, "(%p)__newindex metamethod not implemented", (void*)this);
		return 0;
	}

	int Table::__call(lua_State *L)
	{
		luaL_error(L, "(%p)__call metamethod not implemented", (void*)this);
		return 0;
	}

	int Table::__garbageCollector(lua_State *L)
	{
		luaL_error(L, "(%p) __gc metamethod not implemented", (void*)this);
		return 0;
	}

	int Table::__tostring(lua_State *L)
	{
		int top = lua_gettop(L);
		lua_pushfstring(L, "Table(%p) [%s] with keys:", this, typeid(*this).name());
		for(Elements::iterator i = _elements.begin(); i != _elements.end(); ++i)
		{
			lua_pushfstring(L, "\n\t%s -> %p [%s]",i->first.c_str(), i->second.get(), typeid(*(i->second.get())).name());
		}
		lua_concat(L, lua_gettop(L) - top);
		return 1;
	}

	void Table::pushImplementation(lua_State *L)
	{
		lua_newtable(L); // an empty table

		lua_newtable(L); // metatable

		// table of elements
		lua_newtable(L);
			lua_newtable(L); // meta-table of table of elements
			pushMeta(L, &Table::__index);
			lua_setfield(L, -2, "__index");
		lua_setmetatable(L,-2);
		// set the table of elements as __index table of metatable
		lua_setfield(L,-2, "__index");
		

		pushMeta(L, &Table::__newindex);
		lua_setfield(L, -2, "__newindex");
		pushMeta(L, &Table::__tostring);
		lua_setfield(L, -2, "__tostring");
		pushMeta(L, &Table::__call);
		lua_setfield(L, -2, "__call");
		pushMeta(L, &Table::__garbageCollector);
		lua_setfield(L, -2, "__gc");

		lua_setmetatable(L,-2);
	}
	
	int Table::__meta(lua_State *L)
	{
		Table *table = reinterpret_cast<Table*>(lua_touserdata(L, lua_upvalueindex(1)));
		TableMember member = *reinterpret_cast<TableMember*>(lua_touserdata(L, lua_upvalueindex(2)));
		return (table->*member)(L);
	}
	
	void Table::pushMeta(lua_State *L, Table::TableMember member)
	{
		lua_pushlightuserdata(L, this);
		void *p = lua_newuserdata(L, sizeof(TableMember)); 
		memcpy(p,&member,sizeof(TableMember));
		lua_pushcclosure(L, __meta, 2);
	}

}
