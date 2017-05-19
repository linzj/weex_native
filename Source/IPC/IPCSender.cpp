#include "IPCSender.h"
#include "Buffering/IPCBuffer.h"
#include "IPCArguments.h"
#include "IPCByteArray.h"
#include "IPCCheck.h"
#include "IPCCommunicator.h"
#include "IPCException.h"
#include "IPCHandler.h"
#include "IPCResult.h"
#include "IPCString.h"
#include "Serializing/IPCSerializer.h"
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <vector>

namespace {
class IPCSenderImpl : public IPCCommunicator, public IPCSender {
public:
    IPCSenderImpl(int fd, IPCHandler* handler, bool ownFd);
    ~IPCSenderImpl();
    std::unique_ptr<IPCResult> send(IPCBuffer*) override;

private:
    bool checkBufferAsync(IPCBuffer* buffer);
    IPCHandler* m_handler;
    bool m_ownFd;
};

IPCSenderImpl::IPCSenderImpl(int fd, IPCHandler* handler, bool ownFd)
    : IPCCommunicator(fd)
    , m_handler(handler)
    , m_ownFd(ownFd)
{
}

IPCSenderImpl::~IPCSenderImpl()
{
    if (!m_ownFd)
        releaseFd();
}

std::unique_ptr<IPCResult> IPCSenderImpl::send(IPCBuffer* buffer)
{
    doSendBufferOnly(buffer);
    if (checkBufferAsync(buffer))
        return createVoidResult();
    while (true) {
        uint32_t msg = doReadPackage();
        bool isAsync = !!(msg & MSG_FLAG_ASYNC);
        msg &= MSG_MASK;
        if (msg == MSG_END) {
            std::unique_ptr<IPCResult> result = assembleResult();
            clearBlob();
            return result;
        }
        std::unique_ptr<IPCArguments> arguments = assembleArguments();
        clearBlob();
        std::unique_ptr<IPCResult> sendBack = m_handler->handle(msg, arguments.get());
        if (!isAsync) {
            std::unique_ptr<IPCBuffer> resultBuffer = generateResultBuffer(sendBack.get());
            doSendBufferOnly(resultBuffer.get());
        }
    }
}

bool IPCSenderImpl::checkBufferAsync(IPCBuffer* buffer)
{
    uint32_t msg = *static_cast<const uint32_t*>(buffer->get());
    return !!(msg & MSG_FLAG_ASYNC);
}
}

std::unique_ptr<IPCSender> createIPCSender(int fd, IPCHandler* handler, bool ownFd)
{
    return std::unique_ptr<IPCSender>(new IPCSenderImpl(fd, handler, ownFd));
}
