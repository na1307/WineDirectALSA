module;

#include <windows.h>

module casioclassfactory;

import globals;
import casio;

CASIOClassFactory::CASIOClassFactory() : ref_count(0) {
    global_ref_count++;
}

CASIOClassFactory::~CASIOClassFactory() {
    global_ref_count--;
}

HRESULT CASIOClassFactory::QueryInterface(REFIID riid, void **ppvObject) {
    HRESULT hr = S_OK;
    *ppvObject = nullptr;

    if (IsEqualIID(riid, IID_IClassFactory)) {
        *ppvObject = static_cast<IClassFactory*>(this);
    } else if (IsEqualIID(riid, IID_IUnknown)) {
        *ppvObject = static_cast<IUnknown*>(this);
    } else {
        hr = E_NOINTERFACE;
    }

    if (hr == S_OK) {
        static_cast<IUnknown*>(*ppvObject)->AddRef();
    }

    return hr;
}

ULONG CASIOClassFactory::AddRef() {
    return ++ref_count;
}

ULONG CASIOClassFactory::Release() {
    const auto ret = --ref_count;

    if (ref_count == 0) {
        delete this;
    }

    return ret;
}

HRESULT CASIOClassFactory::LockServer(BOOL fLock) {
    fLock ? global_ref_count++ : global_ref_count--;

    return S_OK;
}

HRESULT CASIOClassFactory::CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppvObject) {
    if (pUnkOuter != nullptr) {
        return CLASS_E_NOAGGREGATION;
    }

    if (ppvObject == nullptr) {
        return E_POINTER;
    }

    *ppvObject = nullptr;

    const auto casio = new CASIO;
    const auto hr = casio->QueryInterface(riid, ppvObject);

    if (FAILED(hr)) {
        delete casio;
    }

    return hr;
}
