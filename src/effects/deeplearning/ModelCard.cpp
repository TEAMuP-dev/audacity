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
#include <wx/datetime.h>
#include <wx/file.h>
#include <wx/log.h>

#include <rapidjson/document.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/writer.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/error/en.h>

#include "CodeConversions.h"

namespace validators 
{

   void validateExists(const std::string &key, DocHolder doc)
   {
      if(!doc->IsObject())
         throw InvalidModelCardDocument(XO("The provided JSON document is not an object."), "", doc);
      if(!doc->HasMember(key.c_str()))
      {
         
         throw InvalidModelCardDocument(
            XO("JSON document missing field: %s").Format(wxString(key)), "", doc);
      }
   }

   void throwTypeError(const std::string &key, const char *type, DocHolder doc)
   {
      throw InvalidModelCardDocument(
         XO("field: %s is not of type: %s").Format(wxString(key), wxString(type)), "", doc);
   }

   std::string tryGetString(const std::string &key, DocHolder doc)
   {
      validateExists(key, doc);
      if(!(*doc)[key.c_str()].IsString())
         throwTypeError(key, "string", doc);

      return (*doc)[key.c_str()].GetString();
   }

   std::string tryGetString(const std::string &key, DocHolder doc, const std::string &defaultValue)
   {
      try
      {
         return tryGetString(key, doc);
      }
      catch (const InvalidModelCardDocument &e)
      {
         return defaultValue;
      }
   }

   int tryGetInt(const std::string &key, DocHolder doc)
   {
      validateExists(key, doc);
      if(!(*doc)[key.c_str()].IsInt())
         throwTypeError(key, "int", doc);
      
      return (*doc)[key.c_str()].GetInt();
   }

   int tryGetInt(const std::string &key, DocHolder doc, int defaultValue)
   {
      try
      {
         return tryGetInt(key, doc);
      }
      catch (const InvalidModelCardDocument &e)
      {
         return defaultValue;
      }
   }

   bool tryGetBool(const std::string &key, DocHolder doc)
   {
      validateExists(key, doc);
      if(!(*doc)[key.c_str()].IsBool())
         throwTypeError(key, "bool", doc);
      
      return (*doc)[key.c_str()].GetBool();
   }

   bool tryGetBool(const std::string &key, DocHolder doc, bool defaultValue)
   {
      try
      {
         return tryGetBool(key, doc);
      }
      catch (const InvalidModelCardDocument &e)
      {
         return defaultValue;
      }
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

   std::vector<std::string> tryGetStringArray(const std::string &key, DocHolder doc,
                                             std::vector<std::string> &defaultValue)
   {
      try
      {
         return tryGetStringArray(key, doc); 
      }
      catch (const InvalidModelCardDocument &e)
      {
         return defaultValue;
      }
   }

   uint64_t tryGetUint64(const std::string &key, DocHolder doc)
   {
      validateExists(key, doc);
      if(!(*doc)[key.c_str()].IsUint64())
         throwTypeError(key, "uint64", doc);
      
      return (*doc)[key.c_str()].GetUint64();
   }

   uint64_t tryGetUint64(const std::string &key, DocHolder doc, uint64_t defaultValue)
   {
      try
      {
         return tryGetUint64(key, doc);
      }
      catch (const InvalidModelCardDocument &e)
      {
         return defaultValue;
      }
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
         TranslatableString msg = XO("Error parsing JSON from string:\n%s\nDocument: %s ")
                                       .Format(wxString(rapidjson::GetParseError_En(d->GetParseError())), 
                                                wxString(data));
         throw InvalidModelCardDocument(msg, "", d);
      }
      
      return d;
   }

   DocHolder ParseFile(const std::string &path)
   {
      wxString docStr;
      wxFile file = wxFile(path);

      if(!file.ReadAll(&docStr))
         throw InvalidModelCardDocument(XO("Could not read file."), "", nullptr);

      return ParseString(audacity::ToUTF8(docStr));
   }
}

// ModelCard Implementation

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

      throw InvalidModelCardDocument(Verbatim(message), "", doc);
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
   if (!file.Write(wxString(sb.GetString())))
      throw InvalidModelCardDocument(XO("Could not serialize ModelCard to file"), "", nullptr);
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
   try
   {
      Validate(doc, schema);
   }
   catch(const InvalidModelCardDocument& e)
   {
      wxLogError(e.what());
   }

   // these fields are not in HF mettadata but rather added later,
   // so don't throw if they are not present
   m_author = tryGetString("author", doc, "");
   m_name = tryGetString("name", doc, "");
   m_model_size = (size_t)tryGetUint64("model_size", doc, 0);

   m_long_description = tryGetString("long_description", doc, 
                                       "no long description available");
   m_short_description = tryGetString("short_description", doc, 
                                       "no short description available");
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
    return GetRepoID() == that.GetRepoID();
} 

bool ModelCard::operator!=(const ModelCard& that) const
{
    return !((*this) == that);
}

// ModelCardCollection implementation


void ModelCardCollection::Insert(ModelCardHolder card)
{  
    // only add it if its not already there
   auto it = std::find_if(this->begin(), this->end(), [&](ModelCardHolder a){
      return (*card) == (*a);
   });

   if (it == this->end())
      mCards.push_back(card);
}

ModelCardCollection ModelCardCollection::Filter(ModelCardFilter filter) const
{
   ModelCardCollection that;
   std::copy_if(this->mCards.begin(), this->mCards.end(), 
               std::back_inserter(that.mCards), filter);
   return that;
}
