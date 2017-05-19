#include "IPCListener.h"
#include "Buffering/IPCBuffer.h"
#include "IPCArguments.h"
#include "IPCCommunicator.h"
#include "IPCException.h"
#include "IPCHandler.h"
#include "IPCResult.h"
#include "IPCType.h"
#include <unistd.h>

namespace {
class IPCListenerImpl : public IPCCommunicator,
                        public IPCListener {
public:
    IPCListenerImpl(int fd, IPCHandler* handler);
    ~IPCListenerImpl() override;
    void listen() override;

private:
    int m_fd;
    IPCHandler* m_handler;
};

IPCListenerImpl::IPCListenerImpl(int fd, IPCHandler* handler)
    : IPCCommunicator(fd)
    , m_handler(handler)
{
}

IPCListenerImpl::~IPCListenerImpl()
{
    close(m_fd);
}

void IPCListenerImpl::listen()
{
    while (true) {
        uint32_t msg = doReadPackage();
        bool isAsync = !!(msg & MSG_FLAG_ASYNC);
        msg &= MSG_MASK;
        if (msg == MSG_END)
            throw IPCException("unexpected MSG_END");
        std::unique_ptr<IPCArguments> arguments = assembleArguments();
        clearBlob();
        std::unique_ptr<IPCResult> sendBack = m_handler->handle(msg, arguments.get());
        if (!isAsync) {
            std::unique_ptr<IPCBuffer> resultBuffer = generateResultBuffer(sendBack.get());
            doSendBufferOnly(resultBuffer.get());
        }
    }
}
}

std::unique_ptr<IPCListener> createIPCListener(int fd, IPCHandler* handler)
{
    return std::unique_ptr<IPCListener>(new IPCListenerImpl(fd, handler));
}
