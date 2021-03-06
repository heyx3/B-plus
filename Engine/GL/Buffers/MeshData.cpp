#include "MeshData.h"

using namespace Bplus;
using namespace Bplus::GL;
using namespace Bplus::GL::Buffers;

//TODO: Add Debug-mode code to ensure buffers aren't destroyed before the MeshData instance.


MeshData::MeshData(PrimitiveTypes primType, IndexDataTypes _indexType,
                   const std::optional<MeshDataSource>& _indexData,
                   const std::vector<MeshDataSource>& _vertexBuffers,
                   const std::vector<VertexDataField>& _vertexData)
    : glPtr(GlCreate(glCreateVertexArrays)),
      indexDataType(_indexType), vertexData(_vertexData),
      PrimitiveType(primType)
{
    //Set the vertex and index data sources.
    if (_indexData.has_value())
    {
        indexData = MeshDataSource_Impl{ _indexData.value().Buf->GetOglPtr(),
                                         _indexData.value().DataStructSize,
                                         _indexData.value().InitialByteOffset };
    }
    for (const auto& vertexBufferSrc : _vertexBuffers)
    {
        vertexDataSources.push_back({ vertexBufferSrc.Buf->GetOglPtr(),
                                      vertexBufferSrc.DataStructSize,
                                      vertexBufferSrc.InitialByteOffset });
    }

    //Configure the index buffer in OpenGL.
    if (indexData.has_value())
    {
        glVertexArrayElementBuffer(glPtr.Get(), indexData.value().Buf.Get());
    }
    //Configure the vertex buffer in OpenGL.
    for (size_t i = 0; i < vertexDataSources.size(); ++i)
    {
        glVertexArrayVertexBuffer(glPtr.Get(), (GLuint)i,
                                  vertexDataSources[i].Buf.Get(),
                                  (ptrdiff_t)vertexDataSources[i].InitialByteOffset,
                                  (GLsizei)vertexDataSources[i].DataStructSize);
    }
    //TODO: Can the below be done in one loop instead of three?
    for (size_t i = 0; i < vertexData.size(); ++i)
        glEnableVertexArrayAttrib(glPtr.Get(), (GLuint)i);
    GLuint vertAttribI = 0;
    for (size_t i = 0; i < vertexData.size(); ++i)
    {
        auto fieldOffsetFromStruct = (GLuint)vertexData[i].FieldByteOffset;

        //TODO: Do double vectors/matrices take up twice as many attrib slots as floats? Currently we assume they don't.
        auto fieldType = vertexData[i].FieldType;
        if (fieldType.IsIVector())
        {
            glVertexArrayAttribIFormat(glPtr.Get(), vertAttribI,
                                       fieldType.AsIVector().Size._to_integral(),
                                       fieldType.GetOglEnum(),
                                       fieldOffsetFromStruct);
        }
        else if (fieldType.IsDVector())
        {
            glVertexArrayAttribLFormat(glPtr.Get(), vertAttribI,
                                       fieldType.AsDVector().Size._to_integral(),
                                       fieldType.GetOglEnum(),
                                       fieldOffsetFromStruct);
        }
        else if (fieldType.IsFMatrix())
        {
            for (size_t attr = 0; attr < fieldType.GetNAttributes(); ++attr)
            {
                glVertexArrayAttribFormat(glPtr.Get(), vertAttribI,
                                          fieldType.GetNComponents(), fieldType.GetOglEnum(),
                                          GL_FALSE, fieldOffsetFromStruct);
                vertAttribI += 1;
            }
        }
        else if (fieldType.IsDMatrix())
        {
            for (size_t attr = 0; attr < fieldType.GetNAttributes(); ++attr)
            {
                glVertexArrayAttribLFormat(glPtr.Get(), vertAttribI,
                                           fieldType.GetNComponents(), fieldType.GetOglEnum(),
                                           fieldOffsetFromStruct);
                vertAttribI += 1;
            }
        }
        else //Must be an FVector.
        {
            BP_ASSERT(fieldType.GetLogicalFormat() == +VertexData::LogicalFormats::Vector,
                     "FieldType isn't known");
            glVertexArrayAttribFormat(glPtr.Get(), vertAttribI,
                                      fieldType.GetNComponents(), fieldType.GetOglEnum(),
                                      ( ( fieldType.IsConvertedFVector() &&
                                          fieldType.AsConvertedFVector().Normalize
                                        ) | (
                                          fieldType.IsPackedConvertedFVector() &&
                                          fieldType.AsPackedConvertedFVector().Normalize
                                        )
                                      ) ? GL_TRUE : GL_FALSE,
                                      fieldOffsetFromStruct);
        }

        glVertexArrayBindingDivisor(glPtr.Get(), vertAttribI,
                                    vertexData[i].PerInstance);

        vertAttribI += 1;
    }
    for (size_t i = 0; i < vertexData.size(); ++i)
        glVertexArrayAttribBinding(glPtr.Get(), (GLuint)i, (GLuint)vertexData[i].MeshDataSourceIndex);
}

MeshData::~MeshData()
{
    if (!glPtr.IsNull())
        glDeleteVertexArrays(1, &glPtr.Get());
}


void MeshData::RemoveIndexData()
{
    indexData = std::nullopt;
    glVertexArrayElementBuffer(glPtr.Get(),
                               Bplus::GL::OglPtr::Buffer::null);
}
void MeshData::SetIndexData(const MeshDataSource& _indexData, IndexDataTypes type)
{
    indexData = { _indexData.Buf->GetOglPtr(),
                  _indexData.DataStructSize,
                  _indexData.InitialByteOffset };
    indexDataType = type;

    glVertexArrayElementBuffer(glPtr.Get(),
                               indexData.has_value() ?
                                   indexData.value().Buf.Get() :
                                   OglPtr::Buffer::null);
}

std::optional<MeshDataSource> MeshData::GetIndexData() const
{
    return HasIndexData() ?
               std::make_optional(MeshDataSource{ Buffer::Find(indexData.value().Buf),
                                                  indexData.value().DataStructSize,
                                                  indexData.value().InitialByteOffset }) :
               std::nullopt;
}
std::optional<IndexDataTypes> MeshData::GetIndexDataType() const
{
    return HasIndexData() ? std::make_optional(indexDataType) : std::nullopt;
}

void MeshData::GetVertexData(std::vector<MeshDataSource>& outSources,
                             std::vector<VertexDataField>& outData) const
{
    for (const auto& dataSource : vertexDataSources)
    {
        outSources.push_back({ Buffer::Find(dataSource.Buf),
                               dataSource.DataStructSize, dataSource.InitialByteOffset });
    }

    outData.insert(outData.end(), vertexData.begin(), vertexData.end());
}

void MeshData::Activate() const
{
    //TODO: Keep track of the currently-active mesh, and don't re-bind it twice in a row.
    glBindVertexArray(glPtr.Get());
}