module;

#include <unknwn.h>

export module casioclassfactory;

export class CASIOClassFactory final : public IClassFactory {
    ULONG ref_count;

public:
    CASIOClassFactory();

    CASIOClassFactory(const CASIOClassFactory &) = delete;

    ~CASIOClassFactory();

    HRESULT QueryInterface(REFIID, void **) override;

    ULONG AddRef() override;

    ULONG Release() override;

    HRESULT CreateInstance(IUnknown *, REFIID, void **) override;

    HRESULT LockServer(BOOL) override;
};
