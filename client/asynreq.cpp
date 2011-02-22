static const char *RcsId = "$Id$\n$Name$";

//
// cpp 	- C++ source code file for TANGO AsynReq class
//
// programmer 	- Emmanuel Taurel (taurel@esrf.fr)
//
// original 	- January 2003
//
// Copyright (C) :      2003,2004,2005,2006,2007,2008,2009,2010,2011
//						European Synchrotron Radiation Facility
//                      BP 220, Grenoble 38043
//                      FRANCE
//
// This file is part of Tango.
//
// Tango is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// Tango is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with Tango.  If not, see <http://www.gnu.org/licenses/>.
//
// $Log$
// Revision 3.10  2010/09/09 13:43:38  taurel
// - Add year 2010 in Copyright notice
//
// Revision 3.9  2009/12/18 14:51:01  taurel
// - Safety commit before christmas holydays
// - Many changes to make the DeviceProxy, Database and AttributeProxy
// classes thread safe (good help from the helgrind tool from valgrind)
//
// Revision 3.8  2009/03/18 12:16:56  taurel
// - Fix warnings reported when compiled with the option -Wall
//
// Revision 3.7  2009/02/19 09:23:28  taurel
// - Some changes in DeviceProxy::cancel_asynch_request() method
//
// Revision 3.6  2009/02/19 08:48:51  taurel
// - Add cancel asynchronous request calls to the DeviceProxy class
//
// Revision 3.5  2009/01/21 12:45:15  taurel
// - Change CopyRights for 2009
//
// Revision 3.4  2008/10/06 15:02:17  taurel
// - Changed the licensing info from GPL to LGPL
//
// Revision 3.3  2008/10/02 16:09:25  taurel
// - Add some licensing information in each files...
//
// Revision 3.2  2004/07/07 08:39:55  taurel
//
// - Fisrt commit after merge between Trunk and release 4 branch
// - Add EventData copy ctor, asiignement operator and dtor
// - Add Database and DeviceProxy::get_alias() method
// - Add AttributeProxy ctor from "device_alias/attribute_name"
// - Exception thrown when subscribing two times for exactly yhe same event
//
// Revision 3.1.2.1  2004/03/02 07:40:23  taurel
// - Fix compiler warnings (gcc used with -Wall)
// - Fix bug in DbDatum insertion operator fro vectors
// - Now support "modulo" as periodic filter
//
// Revision 3.1  2003/05/28 14:42:55  taurel
// Add (conditionaly) autoconf generated include file
//
// Revision 3.0  2003/03/25 16:30:46  taurel
// Change revision number to 3.0 before release 3.0.0 of Tango lib
//
// Revision 1.1  2003/03/20 08:56:12  taurel
// New file to support asynchronous calls
//
//

#if HAVE_CONFIG_H
#include <ac_config.h>
#endif

#include <tango.h>

                                                    
namespace Tango
{

//+----------------------------------------------------------------------------
//
// method : 		AsynReq::store_request()
// 
// description : 	Store a new request in the polling request map
//
// argin(s) :		req : The CORBA request object
//			type : The request type
//
// return :		The asynchronous request identifier
//
//-----------------------------------------------------------------------------

long AsynReq::store_request(CORBA::Request_ptr req,TgRequest::ReqType type)
{
//
// If they are some cancelled requests, remove them
//

	omni_mutex_lock sync(*this);
	if (cancelled_request.empty() == false)
	{
		vector<long>::iterator ite;
		bool ret;
		for (ite = cancelled_request.begin();ite != cancelled_request.end();++ite)
		{
			try
			{
				ret = remove_cancelled_request(*ite);
			}
			catch(...) {ret = false;}

			if (ret == true)
			{
				ite = cancelled_request.erase(ite);
				if (ite == cancelled_request.end())
					break;
			}
		}
	}
			
//
// Get a request identifier
//

	long req_id = ui_ptr->get_ident();
	
//
// Store couple ident/request in map
//

	TgRequest tmp_req(req,type);
	
	asyn_poll_req_table.insert(map<long,TgRequest>::value_type(req_id,tmp_req));
	
	return req_id;
}

//+----------------------------------------------------------------------------
//
// method : 		AsynReq::store_request()
// 
// description : 	Store a new request in the callback request maps
//
// argin(s) :		req : The CORBA request object
//			cb : The callback object
//			dev : The device
//			type : The request type
//
//-----------------------------------------------------------------------------


void AsynReq::store_request(CORBA::Request_ptr req,
		   	    CallBack *cb,
		   	    Connection *dev,
		   	    TgRequest::ReqType type)
{
//
// Store couple device_ptr/request in map
//

	TgRequest tmp_req_dev(req,type,cb);
	TgRequest tmp_req(dev,type,cb);
	
	omni_mutex_lock sync(*this);
	cb_dev_table.insert(map<Connection *,TgRequest>::value_type(dev,tmp_req_dev));
	cb_req_table.insert(map<CORBA::Request_ptr,TgRequest>::value_type(req,tmp_req));
	
}

//+----------------------------------------------------------------------------
//
// method : 		AsynReq::get_request()
// 
// description : 	Return the Tango request object associated to the id
//			passed as method argument. This method is used
//			for asynchronous polling mode
//
// argin(s) :		req_id : The Tango request identifier
//
// return :		The Tango request object
//
//-----------------------------------------------------------------------------

Tango::TgRequest &AsynReq::get_request(long req_id)
{
	map<long,TgRequest>::iterator pos;

	omni_mutex_lock sync(*this);	
	pos = asyn_poll_req_table.find(req_id);
	
	if (pos == asyn_poll_req_table.end())
	{
		TangoSys_OMemStream desc;
		desc << "Failed to find a asynchronous polling request ";
		desc << "with id = " << req_id << ends;
                ApiAsynExcept::throw_exception((const char*)"API_BadAsynPollId",
                        		       desc.str(),
					       (const char*)"AsynReq::get_request()");
	}
		
	return pos->second;
}

//+----------------------------------------------------------------------------
//
// method : 		AsynReq::get_request()
// 
// description : 	Return the Tango request object associated to the CORBA
//			request object passed as method argument. 
//			This method is used for asynchronous callback mode
//
// argin(s) :		req : The CORBA request object
//
// return :		The Tango request object
//
//-----------------------------------------------------------------------------

Tango::TgRequest &AsynReq::get_request(CORBA::Request_ptr req)
{
	map<CORBA::Request_ptr,TgRequest>::iterator pos;

	omni_mutex_lock sync(*this);
	pos = cb_req_table.find(req);

	if (pos == cb_req_table.end())
	{
		TangoSys_OMemStream desc;
		desc << "Failed to find a asynchronous callback request ";
                ApiAsynExcept::throw_exception((const char*)"API_BadAsyn",
                        		       desc.str(),
					       (const char*)"AsynReq::get_request() (by request)");
	}
		
	return pos->second;
}

//+----------------------------------------------------------------------------
//
// method : 		AsynReq::get_request()
// 
// description : 	Return the Tango request object associated to the device
//			passed as method argument and for which replies are
//			already arrived. 
//			This method is used for asynchronous callback mode
//
// argin(s) :		dev : The Tango device
//
// return :		The Tango request object
//
//-----------------------------------------------------------------------------

Tango::TgRequest *AsynReq::get_request(Tango::Connection *dev)
{
	multimap<Connection *,TgRequest>::iterator pos;

	bool found = false;
	omni_mutex_lock sync(*this);
	for (pos = cb_dev_table.lower_bound(dev);pos != cb_dev_table.upper_bound(dev);++pos)
	{
		if (pos->second.arrived == true)
		{
			found = true;
			break;
		}
	}	

	if (found == false)
		return NULL;
	else
		return &(pos->second);
}

//+----------------------------------------------------------------------------
//
// method : 		AsynReq::mark_as_arrived()
// 
// description : 	Mark a request as arrived in the callback device map
//
// argin(s) :		req : The CORBA request object
//
//-----------------------------------------------------------------------------

void AsynReq::mark_as_arrived(CORBA::Request_ptr req)
{
	multimap<Connection *,TgRequest>::iterator pos;

	omni_mutex_lock sync(*this);
	for (pos = cb_dev_table.begin();pos != cb_dev_table.end();++pos)
	{
		if (pos->second.request == req)
		{
			pos->second.arrived = true;
			break;
		}
	}
}

//+----------------------------------------------------------------------------
//
// method : 		AsynReq::remove_request()
// 
// description : 	Remove a request from the object map. The Id is passed
//			as input argument. This method is used for the polling
//			mode
//
// argin(s) :		req_id : The Tango request identifier
//
//-----------------------------------------------------------------------------

void AsynReq::remove_request(long req_id)
{
	map<long,TgRequest>::iterator pos;

	omni_mutex_lock sync(*this);	
	pos = asyn_poll_req_table.find(req_id);
	
	if (pos == asyn_poll_req_table.end())
	{
		TangoSys_OMemStream desc;
		desc << "Failed to find a asynchronous polling request ";
		desc << "with id = " << req_id << ends;
                ApiAsynExcept::throw_exception((const char*)"API_BadAsynPollId",
                        		       desc.str(),
					       (const char*)"AsynReq::remove_request()");
	}
	else
	{
		CORBA::release(pos->second.request);
		asyn_poll_req_table.erase(pos);
	}
}

//+----------------------------------------------------------------------------
//
// method : 		AsynReq::remove_cancelled_request()
// 
// description : 	Remove a already cancelled request from the object map. 
//					The Id is passed as input argument.
//
// argin(s) :		req_id : The Tango request identifier
//
// This method returns true if it was possible to remove the request
// (reply already arrived from the server). Otherwise, it returns false
//
//-----------------------------------------------------------------------------

bool AsynReq::remove_cancelled_request(long req_id)
{
	map<long,TgRequest>::iterator pos;

	pos = asyn_poll_req_table.find(req_id);

	bool ret = true;
	
	if (pos != asyn_poll_req_table.end())
	{
		if (pos->second.request->poll_response() == false)
			ret = false;
		else
		{
			CORBA::release(pos->second.request);
			asyn_poll_req_table.erase(pos);
		}
	}

	return ret;
}

//+----------------------------------------------------------------------------
//
// method : 		AsynReq::remove_request()
// 
// description : 	Remove a request from the two map used for asynchronous
//			callback request. This method is used for the callback
//			mode
//
// argin(s) :		dev : The Tango device
//			req : The CORBA request object
//
//-----------------------------------------------------------------------------

void AsynReq::remove_request(Connection *dev,CORBA::Request_ptr req)
{
	multimap<Connection *,TgRequest>::iterator pos;
	map<CORBA::Request_ptr,TgRequest>::iterator pos_req;

	omni_mutex_lock sync(*this);
	for (pos = cb_dev_table.lower_bound(dev);pos != cb_dev_table.upper_bound(dev);++pos)
	{
		if (pos->second.request == req)
		{
			CORBA::release(pos->second.request);
			cb_dev_table.erase(pos);
			break;
		}
	}

	pos_req = cb_req_table.find(req);
	if (pos_req != cb_req_table.end())
	{
		cb_req_table.erase(pos_req);
	}
		
}

//+----------------------------------------------------------------------------
//
// method : 		AsynReq::mark_as_cancelled()
// 
// description : 	Mark a pending polling request as cancelled
//
// argin(s) :		req_id : The request identifier
//
//-----------------------------------------------------------------------------

void AsynReq::mark_as_cancelled(long req_id)
{
	omni_mutex_lock(*this);
	map<long,TgRequest>::iterator pos;

	pos = asyn_poll_req_table.find(req_id);
	
	if (pos != asyn_poll_req_table.end())
	{
		if (find(cancelled_request.begin(),cancelled_request.end(),req_id) == cancelled_request.end())
			cancelled_request.push_back(req_id);
	}
	else
	{
		TangoSys_OMemStream desc;
		desc << "Failed to find a asynchronous polling request ";
		desc << "with id = " << req_id << ends;
		ApiAsynExcept::throw_exception((const char*)"API_BadAsynPollId",
                        		       desc.str(),
									   (const char*)"AsynReq::mark_as_cancelled()");
	}
}

//+----------------------------------------------------------------------------
//
// method : 		AsynReq::mark_all_polling_as_cancelled()
// 
// description : 	Mark all pending polling request as cancelled
//
//-----------------------------------------------------------------------------

void AsynReq::mark_all_polling_as_cancelled()
{
	map<long,TgRequest>::iterator pos;

	omni_mutex_lock sync(*this);	
	for (pos = asyn_poll_req_table.begin();pos != asyn_poll_req_table.end();++pos)
	{
		long id = pos->first;
		if (find(cancelled_request.begin(),cancelled_request.end(),id) == cancelled_request.end())
			cancelled_request.push_back(id);
	}
}

} // End of tango namespace
