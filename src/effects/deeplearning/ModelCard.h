/**********************************************************************

   Audacity: A Digital Audio Editor
   Audacity(R) is copyright (c) 1999-2021 Audacity Team.
   License: GPL v2.  See License.txt.

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

#include "MemoryX.h"

using DocHolder = std::shared_ptr<rapidjson::Document>;

class InvalidModelCardDocument : public std::exception 
{
public:
   InvalidModelCardDocument(const std::string& msg, 
                           DocHolder doc)
                           : m_msg(msg), m_doc(doc) {}
   virtual const char* what() const throw () 
   {
      // TODO: also print the document
      // TODO: check if document is nullptr
      return m_msg.c_str();
      // rapidjson::StringBuffer sb;
      // rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

      // m_doc->Accept(writer);

      // std::stringstream mMsg;
      // mMsg << "Invalid ModelCard document: \n" << mMsg;
      // mMsg << "document: " << std::string(sb.GetString()) << "\n";
      // return mMsg.str();
   }

   const std::string m_msg;
   DocHolder m_doc;
};

namespace parsers 
{
   DocHolder ParseString(const std::string &json);
   DocHolder ParseFile(const std::string &path);
}

class ModelCard
{
public:
   void DeserializeFromFile(const std::string& path);
   void SerializeToFile(const std::string& path) const;

   template < typename Writer >
   void Serialize(Writer &writer) const;
   void Deserialize(DocHolder doc);

   ModelCard();
   ModelCard(const ModelCard&);

public:

   std::string name() const { return m_name; }
   void name(const std::string &name) { m_name = name; }

   std::string author() const { return m_author; }
   void author(const std::string &author) { m_author = author; }

   std::string description() const { return m_description; }
   void description(const std::string &description) { m_description = description; }

   std::string version() const { return m_version; }
   void version(const std::string &version) { m_version = version; }

   int sample_rate() const { return m_sample_rate; }
   void sample_rate(int rate) { m_sample_rate = rate; }

   bool multichannel() const { return m_multichannel; }
   void multichannel(bool multichannel) { m_multichannel = multichannel; }

   std::string effect_type() const { return m_effect_type; }
   void effect_type(const std::string &type) { m_effect_type = type; }

   std::string domain() const { return m_domain; }
   void domain(const std::string &domain) { m_domain = domain; }

   const std::vector<std::string> labels() const { return m_labels; }
   void labels(std::vector<std::string> labels) { m_labels = labels; } // use std::move to not copy

private:
   std::string m_name;
   std::string m_author;
   std::string m_description;
   std::string m_version;
   int m_sample_rate;
   bool m_multichannel;
   std::string m_effect_type;
   std::string m_domain;
   std::vector<std::string> m_labels;

public:

   // TODO: add versioning?
   bool operator==(const ModelCard& that) const;
   bool operator!=(const ModelCard& that) const;

   // returns {author}/{name}
   std::string GetRepoID() const;
};


using ModelCardHolder = std::shared_ptr<ModelCard>;
using ModelCardFilter = std::function<bool(ModelCardHolder card)>;

class ModelCardCollection
{
public:
   // set the JSON schema to be used when Validate() is called. 
   ModelCardCollection();

   // insert a model card into the collection if it matches the schema
   // will throw if the card does not match the schema
   // only adds if the card isn't already there (doesn't throw)
   // @execsafety strong
   void Insert(ModelCardHolder card);

   // returns an empty copy, but with the appropriate schema
   ModelCardCollection EmptyCopy();
   // returns a view of a subset as dictated by the filter
   ModelCardCollection Filter(ModelCardFilter filter);

   // returns an iterator to the cards
   std::vector<ModelCardHolder>::iterator begin() {return mCards.begin();}
   std::vector<ModelCardHolder>::iterator end() {return mCards.end();}
   size_t Size() {return mCards.size();}

   // ModelCard Find(std::string repoID) { return std::find(, repoID);}

private:
   std::vector<ModelCardHolder> mCards;

};