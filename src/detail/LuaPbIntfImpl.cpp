#include "LuaPbIntfImpl.h"

#include "Encoder.h"

// for LuaException
#include <LuaIntf/LuaIntf.h>
#include <LuaIntf/LuaState.h>

#include <google/protobuf/compiler/importer.h>  // for DiskSourceTree
#include <google/protobuf/dynamic_message.h>  // for GetPrototype()
#include <google/protobuf/message.h>  // for Message

#include <sstream>  // for ostringstream

using namespace LuaIntf;

// See CommandLineInterface::Run().

class ErrorCollector : public google::protobuf::compiler::MultiFileErrorCollector
{
public:
    void Clear() { m_sError.clear(); }
    const std::string& GetError() const { return m_sError; }

    // Only record the last error.
    void AddError(const std::string & filename,
        int line, int column, const std::string & message) override
    {
        std::ostringstream oss;
        oss << filename << ":" << line << ": " << message;
        m_sError = oss.str();
    }
private:
    std::string m_sError;
};

LuaPbIntfImpl::LuaPbIntfImpl()
    : m_pDiskSourceTree(new DiskSourceTree),  // unique_ptr
    m_pErrorCollector(new ErrorCollector),  // unique_ptr
    m_pImporter(new Importer(m_pDiskSourceTree.get(),  // unique_ptr
        m_pErrorCollector.get())),
    m_pMsgFactory(new MsgFactory)  // unique_ptr
{
    // The current dir is the default proto path.
    AddProtoPath("");
}

LuaPbIntfImpl::~LuaPbIntfImpl()
{
}

// e.g. AddProtoPath("proto")
// e.g. AddProtoPath("d:/proto")
void LuaPbIntfImpl::AddProtoPath(const string& sProtoPath)
{
    MapPath("", sProtoPath);
}

void LuaPbIntfImpl::MapPath(
    const string& sVirtualPath,
    const string& sDiskPath)
{
    m_pDiskSourceTree->MapPath(sVirtualPath, sDiskPath);
}

// e.g. ImportProtoFile("bar/foo.proto")
void LuaPbIntfImpl::ImportProtoFile(const string& sProtoFile)
{
    m_pErrorCollector->Clear();
    const google::protobuf::FileDescriptor* pDesc =
        m_pImporter->Import(sProtoFile);
    if (pDesc) return;
    throw LuaException("Failed to import: " + m_pErrorCollector->GetError());
}

MessageSptr LuaPbIntfImpl::MakeSharedMessage(const string& sTypeName) const
{
    const google::protobuf::Descriptor* pDesc =
        m_pImporter->pool()->FindMessageTypeByName(sTypeName);
    if (!pDesc) throw LuaException("No message type: " + sTypeName);
    const google::protobuf::Message* pProtoType =
        m_pMsgFactory->GetPrototype(pDesc);
    if (!pProtoType) throw LuaException("No prototype for " + sTypeName);
    return MessageSptr(pProtoType->New());
}


// luaTable may be a normal table,
// or a message proxy table which has a C++ MessageSptr object,
std::string LuaPbIntfImpl::Encode(const string& sMsgTypeName,
    const LuaRef& luaTable) const
{
    luaTable.checkTable();  // Bad argument #-1 to 'encode' (table expected, got number)
    return Encoder(*this).Encode(sMsgTypeName, luaTable);
}  // Encode()
