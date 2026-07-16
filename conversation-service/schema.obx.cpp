// Manually generated ObjectBox schema implementation for Conversation entity

#include "schema.obx.hpp"

// Property definitions for Conversation (Entity ID: 1, maps to "conversations")
const obx::Property<Conversation, OBXPropertyType_Long> Conversation_::id(1);
const obx::Property<Conversation, OBXPropertyType_String> Conversation_::conversation_id(2);
const obx::Property<Conversation, OBXPropertyType_String> Conversation_::user_id(3);
const obx::Property<Conversation, OBXPropertyType_Long> Conversation_::timestamp(4);
const obx::Property<Conversation, OBXPropertyType_String> Conversation_::role(5);
const obx::Property<Conversation, OBXPropertyType_String> Conversation_::message(6);
const obx::Property<Conversation, OBXPropertyType_String> Conversation_::sources(7);
const obx::Property<Conversation, OBXPropertyType_Long> Conversation_::syncClock(8);
const obx::Property<Conversation, OBXPropertyType_String> Conversation_::tools_used(9);

// Conversation serialization
void Conversation::_OBX_MetaInfo::toFlatBuffer(flatbuffers::FlatBufferBuilder& fbb, const Conversation& object) {
    fbb.Clear();
    auto offset_conversation_id = fbb.CreateString(object.conversation_id);
    auto offset_user_id = fbb.CreateString(object.user_id);
    auto offset_role = fbb.CreateString(object.role);
    auto offset_message = fbb.CreateString(object.message);
    auto offset_sources = fbb.CreateString(object.sources);
    auto offset_tools_used = fbb.CreateString(object.tools_used);

    flatbuffers::uoffset_t fbStart = fbb.StartTable();
    fbb.AddElement(4, object.id);  // Property 1: id
    fbb.AddOffset(6, offset_conversation_id);  // Property 2: conversation_id
    fbb.AddOffset(8, offset_user_id);  // Property 3: user_id
    fbb.AddElement(10, object.timestamp);  // Property 4: timestamp
    fbb.AddOffset(12, offset_role);  // Property 5: role
    fbb.AddOffset(14, offset_message);  // Property 6: message
    fbb.AddOffset(16, offset_sources);  // Property 7: sources
    fbb.AddElement(18, object.syncClock);  // Property 8: syncClock
    fbb.AddOffset(20, offset_tools_used);  // Property 9: tools_used
    
    flatbuffers::Offset<flatbuffers::Table> offset;
    offset.o = fbb.EndTable(fbStart);
    fbb.Finish(offset);
}

Conversation Conversation::_OBX_MetaInfo::fromFlatBuffer(const void* data, size_t size) {
    Conversation object;
    fromFlatBuffer(data, size, object);
    return object;
}

std::unique_ptr<Conversation> Conversation::_OBX_MetaInfo::newFromFlatBuffer(const void* data, size_t size) {
    auto object = std::make_unique<Conversation>();
    fromFlatBuffer(data, size, *object);
    return object;
}

void Conversation::_OBX_MetaInfo::fromFlatBuffer(const void* data, size_t, Conversation& outObject) {
    const auto* table = flatbuffers::GetRoot<flatbuffers::Table>(data);
    assert(table);
    
    // Property 1: id
    outObject.id = table->GetField<int64_t>(4, 0);
    
    // Property 2: conversation_id
    {
        auto* ptr = table->GetPointer<const flatbuffers::String*>(6);
        if (ptr) {
            outObject.conversation_id.assign(ptr->c_str(), ptr->size());
        } else {
            outObject.conversation_id.clear();
        }
    }
    
    // Property 3: user_id
    {
        auto* ptr = table->GetPointer<const flatbuffers::String*>(8);
        if (ptr) {
            outObject.user_id.assign(ptr->c_str(), ptr->size());
        } else {
            outObject.user_id.clear();
        }
    }
    
    // Property 4: timestamp
    outObject.timestamp = table->GetField<int64_t>(10, 0);
    
    // Property 5: role
    {
        auto* ptr = table->GetPointer<const flatbuffers::String*>(12);
        if (ptr) {
            outObject.role.assign(ptr->c_str(), ptr->size());
        } else {
            outObject.role.clear();
        }
    }
    
    // Property 6: message
    {
        auto* ptr = table->GetPointer<const flatbuffers::String*>(14);
        if (ptr) {
            outObject.message.assign(ptr->c_str(), ptr->size());
        } else {
            outObject.message.clear();
        }
    }
    
    // Property 7: sources
    {
        auto* ptr = table->GetPointer<const flatbuffers::String*>(16);
        if (ptr) {
            outObject.sources.assign(ptr->c_str(), ptr->size());
        } else {
            outObject.sources.clear();
        }
    }
    
    // Property 8: syncClock
    outObject.syncClock = table->GetField<int64_t>(18, 0);

    // Property 9: tools_used
    {
        auto* ptr = table->GetPointer<const flatbuffers::String*>(20);
        if (ptr) {
            outObject.tools_used.assign(ptr->c_str(), ptr->size());
        } else {
            outObject.tools_used.clear();
        }
    }
}
