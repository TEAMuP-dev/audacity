/**********************************************************************

   Audacity: A Digital Audio Editor
   Audacity(R) is copyright (c) 1999-2021 Audacity Team.
   License: GPL v2.  See License.txt.

   ModelCard.cpp
   Hugo Flores Garcia

******************************************************************/

#include "ModelCard.h"
#include "DeepModel.h"

#include <wx/string.h>
#include <wx/file.h>
#include <wx/log.h>

#include <rapidjson/document.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/writer.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/error/en.h>


namespace validators 
{

   void validateExists(const std::string &key, DocHolder doc)
   {
      if(!doc->IsObject())
         throw InvalidModelCardDocument("provided document is not an object", doc);
      if(!doc->HasMember(key.c_str()))
      {
         wxString msg = wxString("JSON document missing field: %s").Format(wxString(key));
         throw InvalidModelCardDocument(msg.ToStdString(), doc);
      }
   }

   void throwTypeError(const std::string &key, const char *type, DocHolder doc)
   {
      wxString msg = wxString("field: %s is not of type: %s").Format(wxString(key), wxString(type));
      throw InvalidModelCardDocument(msg.ToStdString(), doc);
   }

   std::string tryGetString(const std::string &key, DocHolder doc)
   {
      validateExists(key, doc);
      if(!(*doc)[key.c_str()].IsString())
         throwTypeError(key, "string", doc);

      return (*doc)[key.c_str()].GetString();
   }

   int tryGetInt(const std::string &key, DocHolder doc)
   {
      validateExists(key, doc);
      if(!(*doc)[key.c_str()].IsInt())
         throwTypeError(key, "int", doc);
      
      return (*doc)[key.c_str()].GetInt();
   }

   bool tryGetBool(const std::string &key, DocHolder doc)
   {
      validateExists(key, doc);
      if(!(*doc)[key.c_str()].IsBool())
         throwTypeError(key, "bool", doc);
      
      return (*doc)[key.c_str()].GetBool();
   }

   std::vector<std::string> tryGetStringArray(const std::string &key, DocHolder doc)
   {
      validateExists(key, doc);
      if(!(*doc)[key.c_str()].IsArray())
         throwTypeError(key, "array", doc);
      
      std::vector<std::string> labels;
      for (auto itr = (*doc)[key.c_str()].Begin(); itr != (*doc)[key.c_str()].End();  ++itr)
         labels.emplace_back(itr->GetString());

      return labels;
   }

   uint64_t tryGetUint64(const std::string &key, DocHolder doc)
   {
      validateExists(key, doc);
      if(!(*doc)[key.c_str()].IsUint64())
         throwTypeError(key, "uint64", doc);
      
      return (*doc)[key.c_str()].GetUint64();
   }

}

namespace parsers 
{
   DocHolder ParseString(const std::string &data)
   {
      DocHolder d = std::make_shared<rapidjson::Document>();
      // parse the data
      d->Parse(data.c_str());
      if (d->Parse(data.c_str()).HasParseError()) 
      {
         std::string msg = "error parsing JSON from string: \n";
         msg += rapidjson::GetParseError_En(d->GetParseError());
         msg += "\n document: \n\t" + std::string(data.c_str()) + "\n";
         throw InvalidModelCardDocument(msg.c_str(), d);
      }
      
      return d;
   }

   DocHolder ParseFile(const std::string &path)
   {
      std::shared_ptr<wxString> docStr = std::make_shared<wxString>();
      wxFile file = wxFile(path);

      if(!file.ReadAll(docStr.get()))
         throw InvalidModelCardDocument("could not read file", nullptr);

      return ParseString(docStr->ToStdString());
   }
}

// ModelCard Implementation

// copy constructor
ModelCard::ModelCard(const ModelCard &other)
: m_name(other.name()),
  m_author(other.author()),
  m_long_description(other.long_description()),
  m_short_description(other.short_description()),
  m_sample_rate(other.sample_rate()),
  m_multichannel(other.multichannel()),
  m_effect_type(other.effect_type()),
  m_domain_tags(other.domain_tags()),
  m_tags(other.tags()),
  m_labels(other.labels())
{
   
}

ModelCard::ModelCard()
{
}

void ModelCard::Validate(DocHolder doc, DocHolder schema)
{
   rapidjson::SchemaDocument schemaDoc(*schema);
   rapidjson::SchemaValidator validator(schemaDoc);

   if (!doc->Accept(validator)) 
   {
      // Input JSON is invalid according to the schema
      // Output diagnostic information
      std::string message("A Schema violation was found in the Model Card.\n");

      rapidjson::StringBuffer sb;
      validator.GetInvalidSchemaPointer().StringifyUriFragment(sb);
      message += "violation found in URI: " + std::string(sb.GetString()) + "\n";
      message += "the following schema field was violated: " 
                  + std::string(validator.GetInvalidSchemaKeyword()) + "\n";
      sb.Clear();

      rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
      doc->Accept(writer);
      message += "invalid document: \n\t" 
                  + std::string(sb.GetString()) + "\n";

      sb.Clear();
      rapidjson::Writer<rapidjson::StringBuffer> swriter(sb);
      schema->Accept(swriter);
      message += "schema document: \n\t" 
                  + std::string(sb.GetString()) + "\n";

      throw InvalidModelCardDocument(message.c_str(), doc);
   }
}

void ModelCard::SerializeToFile(const std::string &path) const
{
   rapidjson::StringBuffer sb;
   rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

   writer.StartObject();
   Serialize(writer);
   writer.EndObject();

   wxFile file(path, wxFile::write);
   file.Write(wxString(sb.GetString()));
}

void ModelCard::DeserializeFromFile(const std::string &path, DocHolder schema)
{
   DocHolder d = parsers::ParseFile(path);
   Deserialize(d, schema);
}

template < typename Writer >
void ModelCard::Serialize(Writer &writer) const
{
   // name
   writer.String("name");
   writer.String(m_name.c_str());

   // author
   writer.String("author");
   writer.String(m_author.c_str());
   
   // long description
   writer.String("long_description");
   writer.String(m_long_description.c_str());

   // short description
   writer.String("short_description");
   writer.String(m_short_description.c_str());

   // sample rate
   writer.String("sample_rate");
   writer.Int(m_sample_rate);

   // multichannel
   writer.String("multichannel");
   writer.Bool(m_multichannel);

   // effect type
   writer.String("effect_type");
   writer.String(m_effect_type.c_str());

   // domain tags
   writer.String("domain_tags");
   writer.StartArray();
   for (auto tag : m_domain_tags)
      writer.String(tag.c_str());
   writer.EndArray();

   // tags
   writer.String("tags");
   writer.StartArray();
   for (auto tag : m_tags)
      writer.String(tag.c_str());
   writer.EndArray();

   // labels
   writer.String("labels");
   writer.StartArray();
   for(auto label : m_labels)
      writer.String(label.c_str());
   writer.EndArray();

   // model size
   writer.String("model_size");
   writer.Uint64((uint64_t)m_model_size);

}

void ModelCard::Deserialize(DocHolder doc, DocHolder schema)
{
   using namespace validators;
   Validate(doc, schema);

   // these fields are not in HF metadata but rather added later,
   // so don't throw if they are not present
   try
   {
      m_author = tryGetString("author", doc);
      m_name = tryGetString("name", doc);
      m_model_size = (size_t)tryGetUint64("model_size", doc);
   }
   catch (const InvalidModelCardDocument &e)
   {
      wxLogDebug(e.what());
   }

   m_long_description = tryGetString("long_description", doc);
   m_short_description = tryGetString("short_description", doc);
   m_effect_type = tryGetString("effect_type", doc);
   m_domain_tags = tryGetStringArray("domain_tags", doc);
   m_tags = tryGetStringArray("tags", doc);
   m_labels = tryGetStringArray("labels", doc);
   m_sample_rate = tryGetInt("sample_rate", doc);
   m_multichannel = tryGetBool("multichannel", doc);
}

std::string ModelCard::GetRepoID() const
{
   return this->author() + '/' + this->name();
}

bool ModelCard::operator==(const ModelCard& that) const
{
    return (*this).GetRepoID() == that.GetRepoID();
} 

bool ModelCard::operator!=(const ModelCard& that) const
{
    return !((*this) == that);
}

// ModelCardCollection implementation

ModelCardCollection::ModelCardCollection()
{
}

void ModelCardCollection::Insert(ModelCardHolder card)
{
   // a lambda that dereferences the ModelCardHolder
   // and checks if the contents are equal
   auto isSame = [&](ModelCardHolder a)
   {
      return (*card) == (*a);
   };
   
    // only add it if its new
   auto it = std::find_if(this->begin(), this->end(), isSame);

   bool isMissing = (it == this->end());
   if (isMissing)
      mCards.push_back(card);
}

ModelCardCollection ModelCardCollection::Filter(ModelCardFilter filter)
{
   ModelCardCollection that = EmptyCopy();
   std::vector<ModelCardHolder>::iterator it = std::copy_if(this->begin(), this->end(), that.begin(), filter);
   that.mCards.resize(std::distance(that.begin(), it));

   return that;
}

ModelCardCollection ModelCardCollection::EmptyCopy()
{
   ModelCardCollection that;
   that.mCards = std::vector<ModelCardHolder>(mCards.size());
   return that;
}
