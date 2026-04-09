// Manually generated ObjectBox schema implementation for ManualChunk and Manual entities

#include "schema.obx.hpp"

// Property definitions for ManualChunk (Entity ID: 1, maps to "manual_chunks")
const obx::Property<ManualChunk, OBXPropertyType_Long> ManualChunk_::id(1);
const obx::Property<ManualChunk, OBXPropertyType_String> ManualChunk_::text(2);
const obx::Property<ManualChunk, OBXPropertyType_String> ManualChunk_::source_file(3);
const obx::Property<ManualChunk, OBXPropertyType_Int> ManualChunk_::chunk_index(4);
const obx::Property<ManualChunk, OBXPropertyType_FloatVector> ManualChunk_::embedding(5);
const obx::Property<ManualChunk, OBXPropertyType_Long> ManualChunk_::syncClock(6);

// ManualChunk serialization
void ManualChunk::_OBX_MetaInfo::toFlatBuffer(flatbuffers::FlatBufferBuilder& fbb, const ManualChunk& object) {
    fbb.Clear();
    auto offsettext = fbb.CreateString(object.text);
    auto offsetsource_file = fbb.CreateString(object.source_file);
    auto offsetembedding = fbb.CreateVector(object.embedding);
    
    flatbuffers::uoffset_t fbStart = fbb.StartTable();
    fbb.AddElement(4, object.id);  // Property 1: id
    fbb.AddOffset(6, offsettext);  // Property 2: text
    fbb.AddOffset(8, offsetsource_file);  // Property 3: source_file
    fbb.AddElement(10, object.chunk_index);  // Property 4: chunk_index
    fbb.AddOffset(12, offsetembedding);  // Property 5: embedding
    fbb.AddElement(14, object.syncClock);  // Property 6: syncClock
    
    flatbuffers::Offset<flatbuffers::Table> offset;
    offset.o = fbb.EndTable(fbStart);
    fbb.Finish(offset);
}

ManualChunk ManualChunk::_OBX_MetaInfo::fromFlatBuffer(const void* data, size_t size) {
    ManualChunk object;
    fromFlatBuffer(data, size, object);
    return object;
}

std::unique_ptr<ManualChunk> ManualChunk::_OBX_MetaInfo::newFromFlatBuffer(const void* data, size_t size) {
    auto object = std::make_unique<ManualChunk>();
    fromFlatBuffer(data, size, *object);
    return object;
}

void ManualChunk::_OBX_MetaInfo::fromFlatBuffer(const void* data, size_t, ManualChunk& outObject) {
    const auto* table = flatbuffers::GetRoot<flatbuffers::Table>(data);
    assert(table);
    
    // Property 1: id
    outObject.id = table->GetField<int64_t>(4, 0);
    
    // Property 2: text
    {
        auto* ptr = table->GetPointer<const flatbuffers::String*>(6);
        if (ptr) {
            outObject.text.assign(ptr->c_str(), ptr->size());
        } else {
            outObject.text.clear();
        }
    }
    
    // Property 3: source_file
    {
        auto* ptr = table->GetPointer<const flatbuffers::String*>(8);
        if (ptr) {
            outObject.source_file.assign(ptr->c_str(), ptr->size());
        } else {
            outObject.source_file.clear();
        }
    }
    
    // Property 4: chunk_index
    outObject.chunk_index = table->GetField<int32_t>(10, 0);
    
    // Property 5: embedding (FloatVector)
    {
        auto* ptr = table->GetPointer<const flatbuffers::Vector<float>*>(12);
        if (ptr) {
            outObject.embedding.assign(ptr->begin(), ptr->end());
        } else {
            outObject.embedding.clear();
        }
    }
    
    // Property 6: syncClock
    outObject.syncClock = table->GetField<int64_t>(14, 0);
}

// Property definitions for Manual (Entity ID: 2, maps to "manuals")
const obx::Property<Manual, OBXPropertyType_Long> Manual_::id(1);
const obx::Property<Manual, OBXPropertyType_String> Manual_::filename(2);
const obx::Property<Manual, OBXPropertyType_String> Manual_::make(3);
const obx::Property<Manual, OBXPropertyType_String> Manual_::model(4);
const obx::Property<Manual, OBXPropertyType_Int> Manual_::total_chunks(5);
const obx::Property<Manual, OBXPropertyType_String> Manual_::status(6);
const obx::Property<Manual, OBXPropertyType_Long> Manual_::syncClock(7);

// Manual serialization
void Manual::_OBX_MetaInfo::toFlatBuffer(flatbuffers::FlatBufferBuilder& fbb, const Manual& object) {
    fbb.Clear();
    auto offsetfilename = fbb.CreateString(object.filename);
   auto offsetmake = fbb.CreateString(object.make);
    auto offsetmodel = fbb.CreateString(object.model);
    auto offsetstatus = fbb.CreateString(object.status);
    
    flatbuffers::uoffset_t fbStart = fbb.StartTable();
    fbb.AddElement(4, object.id);  // Property 1: id
    fbb.AddOffset(6, offsetfilename);  // Property 2: filename
    fbb.AddOffset(8, offsetmake);  // Property 3: make
    fbb.AddOffset(10, offsetmodel);  // Property 4: model
    fbb.AddElement(12, object.total_chunks);  // Property 5: total_chunks
    fbb.AddOffset(14, offsetstatus);  // Property 6: status
    fbb.AddElement(16, object.syncClock);  // Property 7: syncClock
    
    flatbuffers::Offset<flatbuffers::Table> offset;
    offset.o = fbb.EndTable(fbStart);
    fbb.Finish(offset);
}

Manual Manual::_OBX_MetaInfo::fromFlatBuffer(const void* data, size_t size) {
    Manual object;
    fromFlatBuffer(data, size, object);
    return object;
}

std::unique_ptr<Manual> Manual::_OBX_MetaInfo::newFromFlatBuffer(const void* data, size_t size) {
    auto object = std::make_unique<Manual>();
    fromFlatBuffer(data, size, *object);
    return object;
}

void Manual::_OBX_MetaInfo::fromFlatBuffer(const void* data, size_t, Manual& outObject) {
    const auto* table = flatbuffers::GetRoot<flatbuffers::Table>(data);
    assert(table);
    
    // Property 1: id
    outObject.id = table->GetField<int64_t>(4, 0);
    
    // Property 2: filename
    {
        auto* ptr = table->GetPointer<const flatbuffers::String*>(6);
        if (ptr) {
            outObject.filename.assign(ptr->c_str(), ptr->size());
        } else {
            outObject.filename.clear();
        }
    }
    
    // Property 3: make
    {
        auto* ptr = table->GetPointer<const flatbuffers::String*>(8);
        if (ptr) {
            outObject.make.assign(ptr->c_str(), ptr->size());
        } else {
            outObject.make.clear();
        }
    }
    
    // Property 4: model
    {
        auto* ptr = table->GetPointer<const flatbuffers::String*>(10);
        if (ptr) {
            outObject.model.assign(ptr->c_str(), ptr->size());
        } else {
            outObject.model.clear();
        }
    }
    
    // Property 5: total_chunks
    outObject.total_chunks = table->GetField<int32_t>(12, 0);
    
    // Property 6: status
    {
        auto* ptr = table->GetPointer<const flatbuffers::String*>(14);
        if (ptr) {
            outObject.status.assign(ptr->c_str(), ptr->size());
        } else {
            outObject.status.clear();
        }
    }
    
    // Property 7: syncClock
    outObject.syncClock = table->GetField<int64_t>(16, 0);
}
