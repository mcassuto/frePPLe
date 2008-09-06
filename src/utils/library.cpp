/***************************************************************************
  file : $URL$
  version : $LastChangedRevision$  $LastChangedBy$
  date : $LastChangedDate$
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 * Copyright (C) 2007 by Johan De Taeye                                    *
 *                                                                         *
 * This library is free software; you can redistribute it and/or modify it *
 * under the terms of the GNU Lesser General Public License as published   *
 * by the Free Software Foundation; either version 2.1 of the License, or  *
 * (at your option) any later version.                                     *
 *                                                                         *
 * This library is distributed in the hope that it will be useful,         *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser *
 * General Public License for more details.                                *
 *                                                                         *
 * You should have received a copy of the GNU Lesser General Public        *
 * License along with this library; if not, write to the Free Software     *
 * Foundation Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA *
 *                                                                         *
 ***************************************************************************/

#define FREPPLE_CORE
#include "frepple/utils.h"
#include <sys/stat.h>


namespace frepple
{

// Repository of all categories and commands
DECLARE_EXPORT const MetaCategory* MetaCategory::firstCategory = NULL;
DECLARE_EXPORT MetaCategory::CategoryMap MetaCategory::categoriesByTag;
DECLARE_EXPORT MetaCategory::CategoryMap MetaCategory::categoriesByGroupTag;

// Repository of loaded modules
DECLARE_EXPORT set<string> CommandLoadLibrary::registry;

// Command metadata
DECLARE_EXPORT const MetaCategory Command::metadata;
DECLARE_EXPORT const MetaClass CommandList::metadata,
  CommandSystem::metadata,
  CommandLoadLibrary::metadata,
  CommandSetEnv::metadata;

// Processing instruction metadata
DECLARE_EXPORT const MetaCategory XMLinstruction::metadata;

// Home directory
DECLARE_EXPORT string Environment::home("[unspecified]");

// Number of processors.
// The value initialized here is overwritten in the library initialization.
DECLARE_EXPORT int Environment::processors = 1;

// Output logging stream, whose input buffer is shared with either
// Environment::logfile or cout.
DECLARE_EXPORT ostream logger(cout.rdbuf());

// Output file stream
DECLARE_EXPORT ofstream Environment::logfile;

// Name of the log file
DECLARE_EXPORT string Environment::logfilename;

// Hash value computed only once
DECLARE_EXPORT const hashtype MetaCategory::defaultHash(Keyword::hash("default"));


DECLARE_EXPORT void Environment::setHomeDirectory(const string dirname)
{
  // Check if the parameter is the name of a directory
  struct stat stat_p;
  if (stat(dirname.c_str(), &stat_p))
    // Can't verify the status, directory doesn't exist
    throw RuntimeException("Home directory '" + dirname + "' doesn't exist");
  else if (stat_p.st_mode & S_IFDIR)
    // Ok, valid directory
    home = dirname;
  else
    // Exists but it's not a directory
    throw RuntimeException("Invalid home directory '" + dirname + "'");

  // Make sure the directory ends with a slash
  if (!home.empty() && *home.rbegin() != '/') home += '/';
}


DECLARE_EXPORT void Environment::resolveEnvironment(string& s)
{
  for (string::size_type startpos = s.find("${", 0);
      startpos < string::npos;
      startpos = s.find_first_of("${", startpos))
  {
    // Find closing "}"
    string::size_type endpos = s.find_first_of("}", startpos);
    if (endpos >= string::npos)
      throw DataException("Invalid variable expansion in '" + s + "'");

    // Search variable name
    string var(s, startpos+2, endpos - startpos - 2);
    if (var.empty())
      throw DataException("Invalid variable expansion in '" + s + "'");

    // Pick up the environment variable
    char *c = getenv(var.c_str());

    // Replace in the string
    if (c) s.replace(startpos, endpos - startpos + 1, c);
    else s.replace(startpos, endpos - startpos + 1, "");

    // Advance to the end of the replaced characters. If the replaced
    // characters would include another ${XX} construct we could get in
    // an infinite loop!
    if (c) startpos += strlen(c);
  }
}


DECLARE_EXPORT void Environment::setLogFile(string x)
{
  // Bye bye message
  if (!logfilename.empty())
    logger << "Stop logging at " << Date::now() << endl;

  // Close an eventual existing log file.
  if (logfile.is_open()) logfile.close();

  // No new logfile specified: redirect to the standard output stream
  if (x.empty() || x == "+")
  {
    logfilename = x;
    logger.rdbuf(cout.rdbuf());
    return;
  }

  // Open the file: either as a new file, either appending to existing file
  if (x[0] != '+') logfile.open(x.c_str(), ios::out);
  else logfile.open(x.c_str()+1, ios::app);
  if (!logfile.good())
  {
    // Redirect to the previous logfile (or cout if that's not possible)
    if (logfile.is_open()) logfile.close();
    logfile.open(logfilename.c_str(), ios::app);
    logger.rdbuf(logfile.is_open() ? logfile.rdbuf() : cout.rdbuf());
    // The log file could not be opened
    throw RuntimeException("Could not open log file '" + x + "'");
  }

  // Store the file name
  logfilename = x;

  // Redirect the log file.
  logger.rdbuf(logfile.rdbuf());

  // Print a nice header
  logger << "Start logging frePPLe " << PACKAGE_VERSION << " ("
    << __DATE__ << ") at " << Date::now() << endl;
}


void LibraryUtils::initialize()
{
  // Initialize only once
  static bool init = false;
  if (init)
  {
    logger << "Warning: Calling frepple::LibraryUtils::initialize() more "
    << "than once." << endl;
    return;
  }
  init = true;

  // Set the locale to the default setting.
  // When not executed, the locale is the "C-locale", which restricts us to
  // ascii data in the input.
  // For Posix platforms the environment variable LC_ALL controls the locale.
  // Most Linux distributions these days have a default locale that supports
  // utf-8 encoding, meaning that every unicode character can be 
  // represented.
  // On windows, the default is the system-default ANSI code page. The number
  // of characters that frePPLe supports on windows is constrained by this...
  setlocale(LC_ALL, "" );

  // Initialize Xerces parser
  xercesc::XMLPlatformUtils::Initialize();

  // Initialize the command metadata.
  Command::metadata.registerCategory("command", "commands");
  CommandList::metadata.registerClass(
    "command",
    "command_list",
    Object::createDefault<CommandList>);
  CommandSystem::metadata.registerClass(
    "command",
    "command_system",
    Object::createDefault<CommandSystem>);
  CommandLoadLibrary::metadata.registerClass(
    "command",
    "command_loadlib",
    Object::createDefault<CommandLoadLibrary>);
  CommandSetEnv::metadata.registerClass(
    "command",
    "command_setenv",
    Object::createDefault<CommandSetEnv>);

  // Initialize the processing instruction metadata.
  XMLinstruction::metadata.registerCategory
    ("instruction", NULL, MetaCategory::ControllerDefault);

  // Query the system for the number of available processors
  // The environment variable NUMBER_OF_PROCESSORS is defined automatically on
  // windows platforms. On other platforms it'll have to be explicitly set
  // since there isn't an easy and portable way of querying this system
  // information.
  const char *c = getenv("NUMBER_OF_PROCESSORS");
  if (c)
  {
    int p = atoi(c);
    Environment::setProcessors(p);
  }

#ifdef HAVE_ATEXIT
  atexit(finalize);
#endif
}


void LibraryUtils::finalize()
{
  // Shut down the Xerces parser
  xercesc::XMLPlatformUtils::Terminate();
}


DECLARE_EXPORT void MetaClass::registerClass (const char* a, const char* b, bool def) const
{
  // Re-initializing isn't okay
  if (category)
    throw LogicException("Reinitializing class '" + type + "' isn't allowed");

  // Find or create the category
  MetaCategory* cat
    = const_cast<MetaCategory*>(MetaCategory::findCategoryByTag(a));

  // Check for a valid category
  if (!cat)
    throw LogicException("Category " + string(a)
        + " not found when registering class " + string(b));

  // Update fields
  MetaClass& me = const_cast<MetaClass&>(*this);
  me.type = b;
  me.typetag = &Keyword::find(b);
  me.category = cat;

  // Update the metadata table
  cat->classes[Keyword::hash(b)] = this;

  // Register this tag also as the default one, if requested
  if (def) cat->classes[Keyword::hash("default")] = this;
}


DECLARE_EXPORT void MetaCategory::registerCategory (const char* a, const char* gr,
    readController f, writeController w) const
{
  // Initialize only once
  if (type != "unspecified")
    throw LogicException("Reinitializing category " + type + " isn't allowed");

  // Update registry
  if (a) categoriesByTag[Keyword::hash(a)] = this;
  if (gr) categoriesByGroupTag[Keyword::hash(gr)] = this;

  // Update fields
  MetaCategory& me = const_cast<MetaCategory&>(*this);
  me.readFunction = f;
  me.writeFunction = w;
  if (a)
  {
    // Type tag
    me.type = a;
    me.typetag = &Keyword::find(a);
  }
  if (gr)
  {
    // Group tag
    me.group = gr;
    me.grouptag = &Keyword::find(gr);
  }

  // Maintain a linked list of all registered categories
  if (!firstCategory)
    firstCategory = this;
  else
  {
    const MetaCategory *i = firstCategory;
    while (i->nextCategory) i = i->nextCategory;
    const_cast<MetaCategory*>(i)->nextCategory = this;
  }
}


DECLARE_EXPORT const MetaCategory* MetaCategory::findCategoryByTag(const char* c)
{
  // Loop through all categories
  CategoryMap::const_iterator i = categoriesByTag.find(Keyword::hash(c));
  return (i!=categoriesByTag.end()) ? i->second : NULL;
}


DECLARE_EXPORT const MetaCategory* MetaCategory::findCategoryByTag(const hashtype h)
{
  // Loop through all categories
  CategoryMap::const_iterator i = categoriesByTag.find(h);
  return (i!=categoriesByTag.end()) ? i->second : NULL;
}


DECLARE_EXPORT const MetaCategory* MetaCategory::findCategoryByGroupTag(const char* c)
{
  // Loop through all categories
  CategoryMap::const_iterator i = categoriesByGroupTag.find(Keyword::hash(c));
  return (i!=categoriesByGroupTag.end()) ? i->second : NULL;
}


DECLARE_EXPORT const MetaCategory* MetaCategory::findCategoryByGroupTag(const hashtype h)
{
  // Loop through all categories
  CategoryMap::const_iterator i = categoriesByGroupTag.find(h);
  return (i!=categoriesByGroupTag.end()) ? i->second : NULL;
}


DECLARE_EXPORT const MetaClass* MetaCategory::findClass(const char* c) const
{
  // Look up in the registered classes
  MetaCategory::ClassMap::const_iterator j = classes.find(Keyword::hash(c));
  return (j == classes.end()) ? NULL : j->second;
}


DECLARE_EXPORT const MetaClass* MetaCategory::findClass(const hashtype h) const
{
  // Look up in the registered classes
  MetaCategory::ClassMap::const_iterator j = classes.find(h);
  return (j == classes.end()) ? NULL : j->second;
}


DECLARE_EXPORT void MetaCategory::persist(XMLOutput *o)
{
  for (const MetaCategory *i = firstCategory; i; i = i->nextCategory)
    if (i->writeFunction) i->writeFunction(*i, o);
}


DECLARE_EXPORT const MetaClass* MetaClass::findClass(const char* c)
{
  // Loop through all categories
  for (MetaCategory::CategoryMap::const_iterator i = MetaCategory::categoriesByTag.begin();
      i != MetaCategory::categoriesByTag.end(); ++i)
  {
    // Look up in the registered classes
    MetaCategory::ClassMap::const_iterator j
      = i->second->classes.find(Keyword::hash(c));
    if (j != i->second->classes.end()) return j->second;
  }
  // Not found...
  return NULL;
}


DECLARE_EXPORT void MetaClass::printClasses()
{
  logger << "Registered classes:" << endl;
  // Loop through all categories
  for (MetaCategory::CategoryMap::const_iterator i = MetaCategory::categoriesByTag.begin();
      i != MetaCategory::categoriesByTag.end(); ++i)
  {
    logger << "  " << i->second->type << endl;
    // Loop through the classes for the category
    for (MetaCategory::ClassMap::const_iterator
        j = i->second->classes.begin();
        j != i->second->classes.end();
        ++j)
      if (j->first == Keyword::hash("default"))
        logger << "    default ( = " << j->second->type << " )" << j->second << endl;
      else
        logger << "    " << j->second->type << j->second << endl;
  }
}


DECLARE_EXPORT Action MetaClass::decodeAction(const char *x)
{
  // Validate the action
  if (!x) throw LogicException("Invalid action NULL");
  else if (!strcmp(x,"AC")) return ADD_CHANGE;
  else if (!strcmp(x,"A")) return ADD;
  else if (!strcmp(x,"C")) return CHANGE;
  else if (!strcmp(x,"R")) return REMOVE;
  else throw LogicException("Invalid action '" + string(x) + "'");
}


DECLARE_EXPORT Action MetaClass::decodeAction(const AttributeList& atts)
{  
  // Decode the string and return the default in the absence of the attribute
  const DataElement* c = atts.get(Tags::tag_action);      
  return *c ? decodeAction(c->getString().c_str()) : ADD_CHANGE;
}


DECLARE_EXPORT bool MetaClass::raiseEvent(Object* v, Signal a) const
{
  bool result(true);
  for (list<Functor*>::const_iterator i = subscribers[a].begin();
      i != subscribers[a].end(); ++i)
    // Note that we always call all subscribers, even if one or more
    // already replied negatively. However, an exception thrown from a
    // callback method will break the publishing chain.
    if (!(*i)->callback(v,a)) result = false;

  // Raise the event also on the category, if there is a valid one
  return (category && category!=this) ?
      (result && category->raiseEvent(v,a)) :
      result;
}


Object* MetaCategory::ControllerDefault (const MetaClass& cat, const AttributeList& in)
{
  Action act = ADD;
  switch (act)
  {
    case REMOVE:
      throw DataException
      ("Entity " + cat.type + " doesn't support REMOVE action.");
    case CHANGE:
      throw DataException
      ("Entity " + cat.type + " doesn't support CHANGE action.");
    default:
      /* Lookup for the class in the map of registered classes. */
      const MetaClass* j;
      if (cat.category)
        // Class metadata passed: we already know what type to create
        j = &cat;
      else
      {
        // Category metadata passed: we need to look up the type
        const DataElement* type = in.get(Tags::tag_type);
        j = static_cast<const MetaCategory&>(cat).findClass(*type ? Keyword::hash(type->getString()) : MetaCategory::defaultHash);
        if (!j)
        {
          string t(*type ? type->getString() : "default");
          throw LogicException("No type " + t + " registered for category " + cat.type);
        }
      }

      // Call the factory method
      Object* result = j->factoryMethodDefault();

      // Run the callback methods
      if (!result->getType().raiseEvent(result, SIG_ADD))
      {
        // Creation denied
        delete result;
        throw DataException("Can't create object");
      }

      // Creation accepted
      return result;
  }
  assert("Unreachable code reached");
  return NULL;
}


void HasDescription::writeElement(XMLOutput *o, const Keyword &t, mode m) const
{
  // Note that this function is never called on its own. It is always called
  // from the writeElement() method of a subclass.
  // Hence, we don't bother about the mode.
  o->writeElement(Tags::tag_category, cat);
  o->writeElement(Tags::tag_subcategory, subcat);
  o->writeElement(Tags::tag_description, descr);
}


void HasDescription::endElement (XMLInput& pIn, const Attribute& pAttr, const DataElement& pElement)
{
  if (pAttr.isA(Tags::tag_category))
    setCategory(pElement.getString());
  else if (pAttr.isA(Tags::tag_subcategory))
    setSubCategory(pElement.getString());
  else if (pAttr.isA(Tags::tag_description))
    setDescription(pElement.getString());
}

}