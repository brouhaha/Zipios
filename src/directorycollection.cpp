/*
  Zipios++ - a small C++ library that provides easy access to .zip files.
  Copyright (C) 2000-2015  Thomas Sondergaard

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
*/

/** \file
 * \brief Implementation of zipios::DirectoryCollection.
 *
 * This file includes the implementation of the zipios::DirectoryCollection
 * class, which is used to read a directory from disk and create
 * a set of zipios::DirectoryEntry objects.
 */

#include "zipios++/directorycollection.hpp"

#include "zipios++/zipiosexceptions.hpp"

#include "directory.hpp"

#include <fstream>


namespace zipios
{

/** \class DirectoryCollection
 * \brief A collection generated from reading a directory.
 *
 * The DirectoryCollection class is a FileCollection that obtains
 * its entries from a directory on disk.
 */


/** \brief Initialize a DirectoryCollection object.
 *
 * The default constructor initializes an empty directory collection.
 * Note that an empty collection is invalid by default so there is
 * probably not much you will be able to do with such an object.
 */
DirectoryCollection::DirectoryCollection()
    //: m_entries_loaded(false) -- auto-init
    //, m_recursive(true) -- auto-init
    //, m_filepath("") -- auto-init
{
}


/** \brief Initialize a DirectoryCollection object.
 *
 * This function initializes a directory which represents a collection
 * of files from disk.
 *
 * By default recursive is true meaning that the specified directory
 * and all of its children are read in the collection.
 *
 * \warning
 * The specified path must be a valid directory path and name. If the
 * name represents a file, then the DirectoryCollection is marked as
 * invalid.
 *
 * \note
 * The file content is not loaded so the collection is fairly lightweight.
 *
 * \param[in] path  A directory path. If the name is not a valid
 *                  directory the created DirectoryCollection is
 *                  marked as being invalid.
 * \param[in] recursive  Whether to load all the files found in
 *                       sub-direcotries.
 */
DirectoryCollection::DirectoryCollection(std::string const& path, bool recursive)
    //: m_entries_loaded(false) -- auto-init
    : m_recursive(recursive)
    , m_filepath(path)
{
    m_filename = m_filepath;
    m_valid = m_filepath.isDirectory();
}


/** \brief Clean up a DirectoryCollection object.
 *
 * The destructor ensures that the object is properly cleaned up.
 */
DirectoryCollection::~DirectoryCollection()
{
}


/** \brief Close the directory collection.
 *
 * This function marks the collection as invalid in effect rendering
 * the collection unusable.
 */
void DirectoryCollection::close()
{
    m_valid = false;

    // for cleanliness, not really required although we will eventually
    // save some memory that way
    m_entries_loaded = false;
    m_entries.clear();
    m_filename = "-";
    m_filepath = "";
}


/** \brief Retrieve a vector to the collection entries.
 *
 * This function makes sure that the directory collection is valid, if not
 * an exception is raised. If valid, then the function makes sure that
 * the entries were loaded and then it returns a copy of the vector
 * holding the entries.
 *
 * \note
 * The copy of the vector is required because of the implementation
 * of CollectionCollection which does not hold a vector of all the
 * entries defined in its children. It is also cleaner (albeit slower)
 * in case one wants to use the library in a thread environment.
 *
 * \return A reference to the internal FileEntry vector.
 */
FileEntry::vector_t DirectoryCollection::entries() const
{
    loadEntries();

    return FileCollection::entries();
}


/** \brief Get an entry from the collection.
 *
 * This function returns a shared pointer to a FileEntry object for
 * the entry with the specified name. To ignore the path part of the
 * filename while searching for a match, specify FileCollection::IGNORE
 * as the second argument.
 *
 * \note
 * The collection must be valid or the function raises an exception.
 *
 * \param[in] name  A string containing the name of the entry to get.
 * \param[in] matchpath  Speficy MatchPath::MATCH, if the path should match
 *                       as well, specify MatchPath::IGNORE, if the path
 *                       should be ignored.
 *
 * \return A ConstEntryPointer to the found entry. The returned pointer
 *         equals zero if no entry is found.
 *
 * \sa mustBeValid()
 */
FileEntry::pointer_t DirectoryCollection::getEntry(std::string const& name, MatchPath matchpath) const
{
    loadEntries();

    return FileCollection::getEntry(name, matchpath);
}


/** \brief Retrieve pointer to an istream.
 *
 * This function returns a shared pointer to an istream defined from
 * the named entry, which is expected to be available in this collection.
 *
 * The function returns a NULL pointer if no FileEntry can be found from
 * the specified name or the FileEntry is marked as invalid.
 *
 * The returned istream represents a file on disk, although the filename
 * must exist in the collection or it will be ignored. A filename that
 * represents a directory cannot return an input stream and thus an error
 * is returned in that case.
 *
 * \note
 * The stream is always opened in binary mode.
 *
 * \param[in] entry_name  The name of the file to search in the collection.
 * \param[in] matchpath  Whether the full path or just the filename is matched.
 *
 * \return A shared pointer to an open istream for the specified entry.
 */
DirectoryCollection::stream_pointer_t DirectoryCollection::getInputStream(std::string const& entry_name, MatchPath matchpath)
{
    FileEntry::pointer_t ent(getEntry(entry_name, matchpath));
    if(!ent || ent->isDirectory())
    {
        return DirectoryCollection::stream_pointer_t();
    }

    DirectoryCollection::stream_pointer_t p(new std::ifstream(ent->getName(), std::ios::in | std::ios::binary));
    return p;
}


/** \brief Return the number of entries defined in this collection.
 *
 * This function makes sure that the DirectoryCollection loaded its
 * entries, then it returns the size of the m_entries vector which
 * represents all the files in this directory, including the root
 * directory.
 *
 * \return The number of entries defined in this collection.
 */
size_t DirectoryCollection::size() const
{
    loadEntries();

    return m_entries.size();
}


/** \brief Create another DirectoryCollection.
 *
 * This function creates a clone of this DirectoryCollection. This is
 * a simple new DirectoryCollection of this collection.
 *
 * \return The function returns a shared pointer of the new collection.
 */
FileCollection::pointer_t DirectoryCollection::clone() const
{
    return FileCollection::pointer_t(new DirectoryCollection(*this));
}


/** \brief This is an internal function that loads the file entries.
 *
 * This function is the top level which starts the process of loading
 * all the files found in the specified directory and sub-directories
 * if the DirectoryCollection was created with the recursive flag
 * set to true (the default.)
 */
void DirectoryCollection::loadEntries() const
{
    // WARNING: this has to stay here because the collection could get close()'s...
    mustBeValid();

    if(!m_entries_loaded)
    {
        m_entries_loaded = true;

        // include the root directory
        FileEntry::pointer_t ent(new DirectoryEntry(m_filepath, ""));
        const_cast<DirectoryCollection *>(this)->m_entries.push_back(ent);

        // now read the data inside that directory
        const_cast<DirectoryCollection *>(this)->load(FilePath());
    }
}


/** \brief This is the function loading all the file entries.
 *
 * This function loads all the file entries found in the specified
 * directory. If the DirectoryCollection was created with the
 * recursive flag, then this function will load sub-directories
 * infinitum.
 *
 * \param[in] subdir  The directory to read.
 */
void DirectoryCollection::load(FilePath const& subdir)
{
    for(boost::filesystem::dir_it it(m_filepath + subdir); it != boost::filesystem::dir_it(); ++it)
    {
        // skip the "." and ".." directories, they are never added to
        // a Zip archive
        if(*it != "." && *it != "..")
        {
            FileEntry::pointer_t ent(new DirectoryEntry(m_filepath + subdir + *it, ""));
            m_entries.push_back(ent);

            if(m_recursive && ent->isDirectory())
            {
                load(subdir + *it);
            }
        }
    }
}


} // zipios namespace
// vim: ts=4 sw=4 et
