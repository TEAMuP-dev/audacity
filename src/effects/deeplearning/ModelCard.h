/**********************************************************************

   Audacity: A Digital Audio Editor
   Audacity(R) is copyright (c) 1999-2021 Audacity Team.

   ModelCard.h
   Hugo Flores Garcia

******************************************************************/
/**

\class ModelCard
\brief model metadata for deep learning models

*/
/*******************************************************************/

#pragma once

#include <torch/script.h>
#include <torch/torch.h>

#include <rapidjson/document.h>
#include <rapidjson/schema.h>
#include "rapidjson/prettywriter.h" 
#include <rapidjson/writer.h>
#include <wx/string.h>
#include <wx/log.h>

#include "MemoryX.h"
#include "AudacityException.h"

using Doc = rapidjson::Document;

// this exception should be caught internally, but we 
// derive from MessageBoxException just in case it needs to 
// get handled by Audacity
class InvalidModelCardDocument final : public MessageBoxException
{
public:
   InvalidModelCardDocument(const TranslatableString msg, std::string trace): 
                           m_msg(msg), 
                           m_trace(std::move(trace)),
                            MessageBoxException {
                               ExceptionType::Internal,
                               XO("Invalid Model Card Document")
                            }
   { 
      wxLogError(m_msg.Translation());
      if (!m_trace.empty()) 
         wxLogError(wxString(m_trace)); 
   }

   // detailed internal error message
   virtual const char* what() const throw () 
   {
      // TODO: also print the document
      // TODO: check if document is nullptr
      return m_msg.Translation().c_str();
      // rapidjson::StringBuffer sb;
      // rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

      // m_doc->Accept(writer);

      // std::stringstream mMsg;
      // mMsg << "Invalid ModelCard document: \n" << mMsg;
      // mMsg << "document: " << std::string(sb.GetString()) << "\n";
      // return mMsg.str();
   }

   // user facing message
   virtual TranslatableString ErrorMessage() const
      { return XO("Model Card Error: %s")
               .Format(m_msg);}

   const TranslatableString m_msg;
   const std::string m_trace;
};

namespace parsers 
{
   Doc ParseString(const std::string &json);
   Doc ParseFile(const std::string &path);
}

// validators that accept a default value are no-throw
namespace validators
{

   std::string tryGetString(const std::string &key, const Doc& doc);
   std::string tryGetString(const std::string &key, const Doc& doc, const std::string &defaultValue);

   int tryGetInt(const std::string &key, const Doc& doc);
   int tryGetInt(const std::string &key, const Doc& doc, int defaultValue);

   std::vector<std::string> tryGetStringArray(const std::string &key, const Doc& doc, bool lower=false);
                                             
   uint64_t tryGetUint64(const std::string &key, const Doc& doc);
   uint64_t tryGetUint64(const std::string &key, const Doc& doc, uint64_t defaultValue);

}

class DeepModelManager;

class ModelCard final
{
public: 
   ModelCard() = default;
   ModelCard(const ModelCard&) = default;

   // returns {author}/{name}
   std::string GetRepoID() const;

   bool IsSame(const ModelCard& other) const { return (*this) == other; };

private:
   friend class DeepModelManager;
   
   // throws InvalidModelCardDocument if the given json is not valid. 
   void DeserializeFromFile(const std::string& path, const Doc& schema);
   void SerializeToFile(const std::string& path) const;

   template < typename Writer >
   void Serialize(Writer &writer) const;
   void Deserialize(const Doc& doc, const Doc& schema);

   void Validate(const Doc& doc, const Doc& schema);

   bool IsLocal() { return m_is_local; }
   void SetLocal(bool local) { m_is_local = local; }

   std::string GetLocalPath() const { return m_local_path; }
   void GetLocalPath(const std::string& path) { m_local_path = path; }

public:

   std::string name() const { return m_name; }
   void name(const std::string &name) { m_name = name; }

   std::string author() const { return m_author; }
   void author(const std::string &author) { m_author = author; }

   std::string long_description() const { return m_long_description; }
   void long_description(const std::string &long_description) { m_long_description = long_description; }

   std::string short_description() const { return m_short_description; }
   void short_description(const std::string &short_description) { m_short_description = short_description; }

   int sample_rate() const { return m_sample_rate; }
   void sample_rate(int rate) { m_sample_rate = rate; }

   bool multichannel() const { return m_multichannel; }
   void multichannel(bool multichannel) { m_multichannel = multichannel; }

   std::string effect_type() const { return m_effect_type; }
   void effect_type(const std::string &type) { m_effect_type = type; }

   const std::vector<std::string> domain_tags() const { return m_domain_tags; }
   void domain_tags(const std::vector<std::string> tags) { m_domain_tags = tags; }

   const std::vector<std::string> tags() const { return m_tags; }
   void tags(const std::vector<std::string> tags) { m_tags = tags; }

   const std::vector<std::string> labels() const { return m_labels; }
   void labels(std::vector<std::string> labels) { m_labels = labels; } // use std::move to not copy

   size_t model_size() const { return m_model_size; }
   void model_size(size_t model_size) { m_model_size = model_size; }

private:
   std::string m_name {""};
   std::string m_author {""};
   std::string m_long_description {""};
   std::string m_short_description {""};
   int m_sample_rate {0};
   bool m_multichannel  {false};
   std::string m_effect_type {""};
   std::vector<std::string> m_domain_tags;
   std::vector<std::string> m_tags;
   std::vector<std::string> m_labels;
   size_t m_model_size {0};

   bool m_is_local {false};
   std::string m_local_path {""};

public:

   // TODO: add versioning?
   bool operator==(const ModelCard& that) const;
   bool operator!=(const ModelCard& that) const;
};

using ModelCardHolder = std::shared_ptr<ModelCard>;
using ModelCardFilter = std::function<bool(ModelCardHolder card)>;

class ModelCardCollection final
{
public:
   // set the JSON schema to be used when Validate() is called. 
   ModelCardCollection() = default;

   // insert a model card into the collection if it matches the schema
   // will throw if the card does not match the schema
   // only adds if the card isn't already there (doesn't throw)
   // @execsafety strong
   void Insert(ModelCardHolder card);

   // returns a view of a subset as dictated by the filter
   ModelCardCollection Filter(ModelCardFilter filter) const;

   // returns an iterator to the cards
   std::vector<ModelCardHolder>::const_iterator begin() const {return mCards.begin();}
   std::vector<ModelCardHolder>::const_iterator end() const {return mCards.end();}
   size_t Size() {return mCards.size();}

   // ModelCard Find(std::string repoID) { return std::find(, repoID);}

private:
   std::vector<ModelCardHolder> mCards;

};
