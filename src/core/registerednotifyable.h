/*
 * Copyright (c) 2010 Yahoo! Inc. All rights reserved. Licensed under
 * the Apache License, Version 2.0 (the "License"); you may not use
 * this file except in compliance with the License. You may obtain a
 * copy of the License at http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * permissions and limitations under the License. See accompanying
 * LICENSE file.
 * 
 * $Id$
 */

#ifndef	_CL_REGISTEREDNOTIFYABLE_H_
#define _CL_REGISTEREDNOTIFYABLE_H_

namespace clusterlib {

/**
 * Interface that must be derived by specific notifyable objects to be
 * registered.
 */
class RegisteredNotifyable
{
  public:
    /**
     * (Do not use unless a registered object) Get appropriate
     * SafeNotifyableMap from the registered object.
     *
     * @return a reference to the map that contains these types of objects
     */
    virtual void setSafeNotifyableMap(
        SafeNotifyableMap &safeNotifyableMap) = 0;

    /**
     * (Do not use unless a registered object) Get appropriate
     * SafeNotifyableMap from the registered object.
     *
     * @return a pointer to the map that contains these types of objects
     */
    virtual SafeNotifyableMap *getSafeNotifyableMap() = 0;

    /**
     * For objects to be registered, they need a unique name.
     *
     * @return the object's unique name
     */
    virtual const std::string &registeredName() const = 0;

    /**
     * Makes sure all the zknodes of an object exists and then
     * allocates a new notifyable from existing repository data and
     * returns it to the user.
     *
     * @param notifyableName the name of the notifyable to load objects for
     * @param notifyableKey the key of the notifyable to load objects for
     * @param parentSP the parent of the new notifyable
     * @return pointer to the new notifyable or NULL if couldn't be found
     */
    virtual boost::shared_ptr<NotifyableImpl> loadNotifyableFromRepository(
        const std::string &notifyableName,
        const std::string &notifyableKey,
        const boost::shared_ptr<NotifyableImpl> &parentSP) = 0;

    /**
     * Creates the necessary zknodes in the repository.  This is part
     * of creating notifyable object.
     *
     * @param notifyableName the name of the notifyable to create objects for
     * @param notifyableKey the key of the notifyable to create objects for
     */
    virtual void createRepositoryObjects(const std::string &notifyableName,
                                         const std::string &notifyableKey) = 0;

    /**
     * Creates the full key from a name.
     *
     * @param parentKey the parent key (if any)
     * @param name the name of the notifyable
     * @return the full key of the notifyable
     */
    virtual std::string generateKey(const std::string &parentKey,
                                    const std::string &name) const = 0;

    /**
     * Check the name of the notifyable for valid names.
     */
    virtual bool isValidName(const std::string &name) const = 0;

    /**
     * (Needs to be thread-safe) Allocate a new notifyable from
     * existing repository data and return it to the user.
     *
     * @param notifyableName the name of the notifyable to load objects for
     * @param notifyableKey the key of the notifyable to load objects for
     * @param parent the parent of the new notifyable
     * @param factoryOps reference to the FactoryOps object
     * @return pointer to the new notifyable or NULL if couldn't be found
     */
    virtual boost::shared_ptr<NotifyableImpl> createNotifyable(
        const std::string &notifyableName,
        const std::string &notifyableKey,
        const boost::shared_ptr<NotifyableImpl> &parent,
        FactoryOps &factoryOps) const = 0;

    /**
     * Name the Zookeeper nodes that are required to be created for
     * this object.
     *
     * @param notifyableName the name of the notifyable to create objects for
     * @param notifyableKey the key of the notifyable to create objects for
     * @return a vector of all the zookeeper node names in order of 
     *         creation/checking
     */
    virtual std::vector<std::string> generateRepositoryList(
        const std::string &notifyableName,
        const std::string &notifyableKey) const = 0;

    /**
     * Checks if the components (assumed from a split) make up a valid
     * key for this object (not if an actual object exists for that key).
     * 
     * @param key A key to test if it matches this RegisteredNotifyable
     * @return true if key is valid, false if not valid
     */
    virtual bool isValidKey(const std::string &key) = 0;

    /**
     * Checks if the components (assumed from a split) make up a valid
     * key for this object (not if an actual object exists for that key).
     * 
     * @param components A vector of components in the key parsed by split
     *                   (i.e. first component should be "")
     * @param elements The number of elements to check with (should 
     *                 be <= components.size()).  If it is -1, then use 
     *                 components.size().
     * @return true if key is valid, false if not valid
     */
    virtual bool isValidKey(const std::vector<std::string> &components, 
                            int32_t elements = -1) = 0;

    /**
     * Try to get this object type represented exactly by this key.
     *
     * @param key should represent this object
     * @param accessType The access permission to get this object
     * @param msecTimeout -1 for wait forever, 0 for return immediately, 
     *        otherwise the number of milliseconds to wait for the lock.
     * @param pNotifyableSP NULL if cannot be found, else the 
     *        object pointer
     * @return True if operation completed within the msecTimeout, 
     *         false otherwise
     */
    virtual bool getObjectFromKey(
        const std::string &key, 
        AccessType accessType,
        int64_t msecTimeout,
        boost::shared_ptr<NotifyableImpl> *pNotifyableSP) = 0;

    /**
     * Try to get this object type represented exactly by these components.
     *
     * @param components A vector of components in the key parsed by split
     *                   (i.e. first component should be "")
     * @param elements The number of elements to check with (should 
     *                 be <= components.size()).  If it is -1, then use 
     *                 components.size().
     * @param accessType The access permission to get this object
     * @param msecTimeout Msecs to wait for locks (-1 for wait forever, 0 for
     *        no waiting)
     * @param pNotifyableSP NULL if cannot be found, else the object pointer
     * @return True if operation completed within the msecTimeout, 
     *         false otherwise
     */
    virtual bool getObjectFromComponents(
        const std::vector<std::string> &components,
        int32_t elements,
        AccessType accessType,
        int64_t msecTimeout,
        boost::shared_ptr<NotifyableImpl> *pNotifyableSP) = 0;
    
    /**
     * Virtual destructor.
     */
    virtual ~RegisteredNotifyable() {}
};

}	/* End of 'namespace clusterlib' */

#endif	/* !_CL_REGISTEREDNOTIFYABLE_H_ */
