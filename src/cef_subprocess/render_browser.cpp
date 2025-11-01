/*
 * Copyright (c) 2025 The Solo Fortress 2 Team, all rights reserved.
 * render_browser.cpp, Helpers to handle V8 (JS engine) contexts and JS objects.
 *
 * Licensed under CC BY-NC 3.0 (Lambda Wars' original license)
 */

#include "cef_cxx20_stubs.h"
#include "client_app.h"
#include "render_browser.h"

#include "render_browser_helpers.h"

static int s_NextCallbackID = 0;


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
FunctionV8Handler::FunctionV8Handler(CefRefPtr<RenderBrowser> renderBrowser) : m_RenderBrowser(renderBrowser)
{

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void FunctionV8Handler::SetFunc(CefRefPtr<CefV8Value> func)
{
	m_Func = func;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool FunctionV8Handler::Execute(const CefString& name,
	CefRefPtr<CefV8Value> object,
	const CefV8ValueList& arguments,
	CefRefPtr<CefV8Value>& retval,
	CefString& exception)
{
	m_RenderBrowser->CallFunction(m_Func, arguments, retval, exception);
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool FunctionWithCallbackV8Handler::Execute(const CefString& name,
	CefRefPtr<CefV8Value> object,
	const CefV8ValueList& arguments,
	CefRefPtr<CefV8Value>& retval,
	CefString& exception)
{
	// Last argument is the callback and should be a function
	if (!arguments.back()->IsFunction())
	{
		exception = CefString("Last argument must be a callback function!");
		return true;
	}
	m_RenderBrowser->CallFunction(m_Func, arguments, retval, exception, arguments.back());
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static bool CefStringLessFunc(const CefString &a, const CefString &b)
{
    return a.ToString() < b.ToString();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
RenderBrowser::RenderBrowser(CefRefPtr<CefBrowser> browser, CefRefPtr<ClientApp> clientApp) : m_Browser(browser), m_ClientApp(clientApp)
{
	m_Objects.SetLessFunc(CefStringLessFunc);
	m_GlobalObjects.SetLessFunc(CefStringLessFunc);
}

//-----------------------------------------------------------------------------
// Purpose: Called once when the browser is destroyed.
//-----------------------------------------------------------------------------
void RenderBrowser::OnDestroyed()
{
	Clear();

	m_Browser = nullptr;
	m_ClientApp = nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void RenderBrowser::SetV8Context(CefRefPtr<CefV8Context> context)
{
	m_Context = context;
	if (!context)
	{
		Clear();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void RenderBrowser::Clear()
{
	m_Context = nullptr;

    m_Objects.RemoveAll();
    m_GlobalObjects.RemoveAll();
    m_Callbacks.Purge();
}

bool RenderBrowser::RegisterObject(CefString identifier, CefRefPtr<CefV8Value> object)
{
    int idx = m_Objects.Find(identifier);
    if (!m_Objects.IsValidIndex(idx))
    {
        idx = m_Objects.Insert(identifier);
    }
    m_Objects[idx] = object;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CefRefPtr<CefV8Value> RenderBrowser::FindObjectForUUID(CefString uuid)
{
	int idx = m_Objects.Find(uuid);
	if (m_Objects.IsValidIndex(idx))
		return m_Objects[idx];
	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool RenderBrowser::CreateGlobalObject(CefString identifier, CefString name)
{
	if (!m_Context || !m_Context->Enter())
		return false;

	bool bRet = false;

	// Retrieve the context's window object.
	CefRefPtr<CefV8Value> object = m_Context->GetGlobal();

	CefRefPtr<CefV8Value> newobject = CefV8Value::CreateObject(nullptr, nullptr);

	// Add the "newobject" object to the "window" object.
	object->SetValue(name, newobject, V8_PROPERTY_ATTRIBUTE_NONE);

	// Remember we created this object
	if (RegisterObject(identifier, newobject))
	{
        int gidx = m_GlobalObjects.Find(name);
        if (!m_GlobalObjects.IsValidIndex(gidx))
        {
            gidx = m_GlobalObjects.Insert(name);
        }
        m_GlobalObjects[gidx] = newobject;
		bRet = true;
	}

	m_Context->Exit();

	return bRet;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool RenderBrowser::CreateFunction(CefString identifier, CefString name, CefString parentIdentifier, bool bCallback)
{
	if (!m_Context || !m_Context->Enter())
		return false;

	// Get object to bind to
	CefRefPtr<CefV8Value> object;
	if (!parentIdentifier.empty())
	{
        int idx = m_Objects.Find(parentIdentifier);
        if (m_Objects.IsValidIndex(idx))
		{
			object = m_Objects[idx];
		}
		else
		{
			m_Context->Exit();
			return false;
		}
	}
	else
	{
		object = m_Context->GetGlobal();
	}

	// Create function and bind to object
    CefRefPtr<FunctionV8Handler> funcHandler = !bCallback ? new FunctionV8Handler(this) : new FunctionWithCallbackV8Handler(this);
    CefRefPtr<CefV8Value> func = CefV8Value::CreateFunction(name, funcHandler.get());
    funcHandler->SetFunc(func);
    object->SetValue(name, func, V8_PROPERTY_ATTRIBUTE_NONE);

	// Register
	if (!RegisterObject(identifier, func))
	{
		m_Context->Exit();
		return false;
	}

	m_Context->Exit();

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool RenderBrowser::ExecuteJavascriptWithResult(CefString identifier, CefString code)
{
	if (!m_Context || !m_Context->Enter())
		return false;

	// Execute code
	CefRefPtr<CefV8Value> retval;
	CefRefPtr<CefV8Exception> exception;
	if (!m_Context->Eval(code, "", 0, retval, exception))
	{
		m_Context->Exit();
		return false;
	}

	// Register object
	if (!RegisterObject(identifier, retval))
	{
		m_Context->Exit();
		return false;
	}

	m_Context->Exit();

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void RenderBrowser::CallFunction(CefRefPtr<CefV8Value> object,
	const CefV8ValueList& arguments,
	CefRefPtr<CefV8Value>& retval,
	CefString& exception,
	CefRefPtr<CefV8Value> callback)
{
    if (!object.get() || !object->IsFunction())
    {
        exception = CefString("Failed to call JavaScript bound function \"" + object->GetFunctionName().ToString() + "\"");
        return;
    }

	// Create message
	CefRefPtr<CefProcessMessage> message = CefProcessMessage::Create("methodcall");
	CefRefPtr<CefListValue> args = message->GetArgumentList();

    if (CefRefPtr<CefBaseRefCounted> userData = object->GetUserData())
    {
		args->SetString(0, object->GetFunctionName());
    }
    else
    {
        args->SetString(0, object->GetFunctionName());
    }

	CefRefPtr<CefListValue> methodargs = CefListValue::Create();
	V8ValueListToListValue(this, arguments, methodargs);

	if (callback)
	{
		// Remove last, this is the callback method
		// Do this before the SetList call
		// SetList will invalidate methodargs and take ownership
		methodargs->Remove(methodargs->GetSize() - 1);
	}

	args->SetList(1, methodargs);

	// Store callback
	if (callback)
	{
        m_Callbacks.AddToTail(jscallback_t());
        int idx = m_Callbacks.Count() - 1;
        m_Callbacks[idx].callback = callback;
        m_Callbacks[idx].callbackid = s_NextCallbackID++;
        m_Callbacks[idx].thisobject = object;

        args->SetInt(2, m_Callbacks[idx].callbackid);
	}
	else
	{
		args->SetNull(2);
	}

	// Send message
	if (m_Browser->GetMainFrame())
		m_Browser->GetMainFrame()->SendProcessMessage(PID_BROWSER, message);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool RenderBrowser::DoCallback(int iCallbackID, CefRefPtr<CefListValue> methodargs)
{
	FOR_EACH_VEC( m_Callbacks, i )
	{
		jscallback_t callback = m_Callbacks[i];
		if (callback.callbackid == iCallbackID)
		{
			// Do callback
			if (m_Context && m_Context->Enter())
			{
				CefV8ValueList args;
				ListValueToV8ValueList(this, methodargs, args);

				CefRefPtr<CefV8Value> result = callback.callback->ExecuteFunction(callback.thisobject, args);
				if (!result)
					m_ClientApp->SendWarning(m_Browser, "Error occurred during calling callback\n");

				m_Context->Exit();
			}
			else
			{
				m_ClientApp->SendWarning(m_Browser, "No context, erasing callback...\n");
			}

			// Remove callback
			m_Callbacks.Remove(i);
			return true;
		}
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool RenderBrowser::Invoke(CefString identifier, CefString methodname, CefRefPtr<CefListValue> methodargs)
{
	if (!m_Context)
		return false;

	// Get object
	CefRefPtr<CefV8Value> object = nullptr;

	if (!identifier.empty())
	{
		int idx = m_Objects.Find(identifier);
		if (!m_Objects.IsValidIndex(idx))
			return false;

		object = m_Objects[idx];
	}

	// Enter context and Make call
	if (!m_Context->Enter())
		return false;

	// Use global if no object was specified
	if (!object)
		object = m_Context->GetGlobal();

	CefRefPtr<CefV8Value> result = nullptr;

	// Get method
	CefRefPtr<CefV8Value> method = object->GetValue(methodname);
	if (method)
	{
		// Execute method
		CefV8ValueList args;
		ListValueToV8ValueList(this, methodargs, args);

		result = method->ExecuteFunction(object, args);
	}

	// Leave context
	m_Context->Exit();

	if (!result)
		return false;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool RenderBrowser::InvokeWithResult(CefString resultIdentifier, CefString identifier, CefString methodname, CefRefPtr<CefListValue> methodargs)
{
	if (!m_Context)
		return false;

	// Get object
	CefRefPtr<CefV8Value> object = nullptr;

	if (!identifier.empty())
	{
		int idx = m_Objects.Find(identifier);
		if (!m_Objects.IsValidIndex(idx))
			return false;

		object = m_Objects[idx];
	}

	// Enter context and Make call
	if (!m_Context->Enter())
		return false;

	// Use global if no object was specified
	if (!object)
		object = m_Context->GetGlobal();

	CefRefPtr<CefV8Value> result = nullptr;

	CefRefPtr<CefV8Value> method = object->GetValue(methodname);
	if (method)
	{
		// Execute method
		CefV8ValueList args;
		ListValueToV8ValueList(this, methodargs, args);

		result = method->ExecuteFunction(object, args);
		if (result)
		{
			// Register result object
			RegisterObject(resultIdentifier, result);
		}
	}

	// Leave context
	m_Context->Exit();

	if (!result)
		return false;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool RenderBrowser::ObjectSetAttr(CefString identifier, CefString attrname, CefRefPtr<CefV8Value> value)
{
	if (!m_Context)
		return false;

	// Get object
	CefRefPtr<CefV8Value> object = nullptr;

	if (!identifier.empty())
	{
		int idx = m_Objects.Find(identifier);
		if (!m_Objects.IsValidIndex(idx))
			return false;

		object = m_Objects[idx];
	}

	// Enter context and Make call
	if (!m_Context->Enter())
		return false;

	bool bRet = object->SetValue(attrname, value, V8_PROPERTY_ATTRIBUTE_NONE);

	// Leave context
	m_Context->Exit();

	return bRet;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool RenderBrowser::ObjectGetAttr(CefString identifier, CefString attrname, CefString resultIdentifier)
{
	if (!m_Context)
		return false;

	// Get object
	CefRefPtr<CefV8Value> object = nullptr;

	if (!identifier.empty())
	{
		int idx = m_Objects.Find(identifier);
		if (!m_Objects.IsValidIndex(idx))
			return false;

		object = m_Objects[idx];
	}

	// Enter context and Make call
	if (!m_Context->Enter())
		return false;

	bool bRet = false;
	CefRefPtr<CefV8Value> result = object->GetValue(attrname);
	if (result)
	{
		RegisterObject(resultIdentifier, result);
		bRet = true;
	}

	// Leave context
	m_Context->Exit();

	return bRet;
}