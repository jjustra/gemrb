/* GemRB - Infinity Engine Emulator
 * Copyright (C) 2003 The GemRB Project
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * $Header: /data/gemrb/cvs2svn/gemrb/gemrb/gemrb/plugins/Core/Variables.cpp,v 1.25 2004/08/28 19:33:01 avenger_teambg Exp $
 *
 */

#include "Variables.h"
#ifndef HAVE_STRNLEN

static int strnlen(const char* string, int maxlen)
{
	if (!string) {
		return -1;
	}
	int i = 0;
	while (maxlen-- > 0) {
		if (!string[i])
			break;
		i++;
	}
	return i;
}
#endif // ! HAVE_STRNLEN

/////////////////////////////////////////////////////////////////////////////
// private inlines 
inline bool Variables::MyCopyKey(char*& dest, const char* key) const
{
	int i, j;

	//use j
	for (i = 0,j = 0; key[i] && j < MAX_VARIABLE_LENGTH - 1; i++)
		if (key[i] != ' ') {
			j++;
		}
	dest = new char[j + 1];
	if (!dest) {
		return false;
	}
	for (i = 0,j = 0; key[i] && j < MAX_VARIABLE_LENGTH - 1; i++) {
		if (key[i] != ' ') {
			dest[j++] = toupper( key[i] );
		}
	}
	dest[j] = 0;
	return true;
}

inline unsigned int Variables::MyHashKey(const char* key) const
{
	unsigned int nHash = 0;
	for (int i = 0; i < key[i] && i < MAX_VARIABLE_LENGTH; i++) {
		//the original engine ignores spaces in variable names
		if (key[i] != ' ')
			nHash = ( nHash << 5 ) + nHash + toupper( key[i] );
	}
	return nHash;
}
/////////////////////////////////////////////////////////////////////////////
// functions
void Variables::GetNextAssoc(POSITION& rNextPosition, const char*& rKey,
	ieDword& rValue) const
{
	MYASSERT( m_pHashTable != NULL );  // never call on empty map

	Variables::MyAssoc* pAssocRet = ( Variables::MyAssoc* ) rNextPosition;
	MYASSERT( pAssocRet != NULL );

	if (pAssocRet == ( Variables::MyAssoc * ) BEFORE_START_POSITION) {
		// find the first association
		for (unsigned int nBucket = 0; nBucket < m_nHashTableSize; nBucket++)
			if (( pAssocRet = m_pHashTable[nBucket] ) != NULL)
				break;
		MYASSERT( pAssocRet != NULL );  // must find something
	}
	Variables::MyAssoc* pAssocNext;
	if (( pAssocNext = pAssocRet->pNext ) == NULL) {
		// go to next bucket
		for (unsigned int nBucket = pAssocRet->nHashValue + 1;
			nBucket < m_nHashTableSize;
			nBucket++)
			if (( pAssocNext = m_pHashTable[nBucket] ) != NULL)
				break;
	}

	rNextPosition = ( POSITION ) pAssocNext;

	// fill in return data
	rKey = pAssocRet->key;
	rValue = pAssocRet->Value.nValue;
}

Variables::Variables(int nBlockSize, int nHashTableSize)
{
	MYASSERT( nBlockSize > 0 );
	MYASSERT( nHashTableSize > 16 );

	m_pHashTable = NULL;
	m_nHashTableSize = nHashTableSize;  // default size
	m_nCount = 0;
	m_lParseKey = false;
	m_pFreeList = NULL;
	m_pBlocks = NULL;
	m_nBlockSize = nBlockSize;
	m_type = GEM_VARIABLES_INT;
}

void Variables::InitHashTable(unsigned int nHashSize, bool bAllocNow)
	//
	// Used to force allocation of a hash table or to override the default
	//   hash table size of (which is fairly small)
{
	MYASSERT( m_nCount == 0 );
	MYASSERT( nHashSize > 16 );

	if (m_pHashTable != NULL) {
		// free hash table
		delete[] m_pHashTable;
		m_pHashTable = NULL;
	}

	if (bAllocNow) {
		m_pHashTable = new Variables::MyAssoc * [nHashSize];
		memset( m_pHashTable, 0, sizeof( Variables::MyAssoc * ) * nHashSize );
	}
	m_nHashTableSize = nHashSize;
}

void Variables::RemoveAll()
{
	if (m_pHashTable != NULL) {
		// destroy elements (values and keys)
		for (unsigned int nHash = 0; nHash < m_nHashTableSize; nHash++) {
			Variables::MyAssoc* pAssoc;
			for (pAssoc = m_pHashTable[nHash];
				pAssoc != NULL;
				pAssoc = pAssoc->pNext) {
				if (m_type == GEM_VARIABLES_STRING) {
					if (pAssoc->Value.sValue) {
						free( pAssoc->Value.sValue );
						pAssoc->Value.sValue = NULL;
					}
				}
				if (pAssoc->key) {
					free(pAssoc->key);
					pAssoc->key = NULL;
				}
			}
		}
	}

	// free hash table
	delete[] m_pHashTable;
	m_pHashTable = NULL;

	m_nCount = 0;
	m_pFreeList = NULL;
	MemBlock* p = m_pBlocks;
	while (p != NULL) {
		MemBlock* pNext = p->pNext;
		delete[] p;
		p = pNext;
	}
	m_pBlocks = NULL;
}

Variables::~Variables()
{
	RemoveAll();
	MYASSERT( m_nCount == 0 );
}

Variables::MyAssoc* Variables::NewAssoc(const char* key)
{
	if (m_pFreeList == NULL) {
		// add another block
		Variables::MemBlock* newBlock = ( Variables::MemBlock* ) new char[m_nBlockSize*sizeof( Variables::MyAssoc ) + sizeof( Variables::MemBlock )];
		newBlock->pNext = m_pBlocks;
		m_pBlocks = newBlock;

		// chain them into free list
		Variables::MyAssoc* pAssoc = ( Variables::MyAssoc* ) ( newBlock + 1 );
		// free in reverse order to make it easier to debug
		pAssoc += m_nBlockSize - 1;
		for (int i = m_nBlockSize - 1; i >= 0; i--, pAssoc--) {
			pAssoc->pNext = m_pFreeList;
			m_pFreeList = pAssoc;
		}
	}
	MYASSERT( m_pFreeList != NULL );  // we must have something
	Variables::MyAssoc* pAssoc = m_pFreeList;
	m_pFreeList = m_pFreeList->pNext;
	m_nCount++;
	MYASSERT( m_nCount > 0 );    // make sure we don't overflow
	if (m_lParseKey) {
		MyCopyKey( pAssoc->key, key );
	} else {
		int len;
		len = strnlen( key, MAX_VARIABLE_LENGTH - 1 );
		pAssoc->key = (char *) malloc(len + 1);
		if (pAssoc->key) {
			memcpy( pAssoc->key, key, len );
			pAssoc->key[len] = 0;
		}
	}
	pAssoc->Value.nValue = 0xcccccccc;  //invalid value
	pAssoc->nHashValue = 0xcccccccc; //invalid value
	return pAssoc;
}

void Variables::FreeAssoc(Variables::MyAssoc* pAssoc)
{
	if (pAssoc->key) {
		free(pAssoc->key);
		pAssoc->key = NULL;
	}
	pAssoc->pNext = m_pFreeList;
	m_pFreeList = pAssoc;
	m_nCount--;
	MYASSERT( m_nCount >= 0 );  // make sure we don't underflow

	// if no more elements, cleanup completely
	if (m_nCount == 0) {
		RemoveAll();
	}
}

Variables::MyAssoc* Variables::GetAssocAt(const char* key, unsigned int& nHash) const
	// find association (or return NULL)
{
	nHash = MyHashKey( key ) % m_nHashTableSize;

	if (m_pHashTable == NULL) {
		return NULL;
	}

	// see if it exists
	Variables::MyAssoc* pAssoc;
	for (pAssoc = m_pHashTable[nHash];
		pAssoc != NULL;
		pAssoc = pAssoc->pNext) {
		if (!strnicmp( pAssoc->key, key, MAX_VARIABLE_LENGTH )) {
			return pAssoc;
		}
	}

	return NULL;
}

int Variables::GetValueLength(const char* key) const
{
	unsigned int nHash;
	Variables::MyAssoc* pAssoc = GetAssocAt( key, nHash );
	if (pAssoc == NULL) {
		return 0;  // not in map
	}

	return ( int ) strlen( pAssoc->Value.sValue );
}

bool Variables::Lookup(const char* key, char* dest, int MaxLength) const
{
	unsigned int nHash;
	MYASSERT( m_type == GEM_VARIABLES_STRING );
	Variables::MyAssoc* pAssoc = GetAssocAt( key, nHash );
	if (pAssoc == NULL) {
		dest[0] = 0;
		return false;  // not in map
	}

	strncpy( dest, pAssoc->Value.sValue, MaxLength );
	return true;
}

bool Variables::Lookup(const char* key, char *&dest) const
{
	unsigned int nHash;
	MYASSERT(m_type==GEM_VARIABLES_STRING);
	Variables::MyAssoc* pAssoc = GetAssocAt( key, nHash );
	if (pAssoc == NULL) {
		return false;
	}  // not in map

	dest = pAssoc->Value.sValue;
	return true;
}

bool Variables::Lookup(const char* key, ieDword& rValue) const
{
	unsigned int nHash;
	MYASSERT(m_type==GEM_VARIABLES_INT);
	Variables::MyAssoc* pAssoc = GetAssocAt( key, nHash );
	if (pAssoc == NULL) {
		return false;
	}  // not in map

	rValue = pAssoc->Value.nValue;
	return true;
}

void Variables::SetAt(const char* key, const char* value)
{
	unsigned int nHash;
	Variables::MyAssoc* pAssoc;

	MYASSERT( m_type == GEM_VARIABLES_STRING );
	if (( pAssoc = GetAssocAt( key, nHash ) ) == NULL) {
		if (m_pHashTable == NULL)
			InitHashTable( m_nHashTableSize );

		// it doesn't exist, add a new Association
		pAssoc = NewAssoc( key );
		// put into hash table
		pAssoc->pNext = m_pHashTable[nHash];
		m_pHashTable[nHash] = pAssoc;
	} else {
		if (pAssoc->Value.sValue) {
			free( pAssoc->Value.sValue );
			pAssoc->Value.sValue = 0;
		}
	}

	//set value only if we have a key
	if (pAssoc->key) {
		pAssoc->Value.sValue = (char *) value;
		pAssoc->nHashValue = nHash;
	}
}

void Variables::SetAt(const char* key, ieDword value)
{
	unsigned int nHash;
	Variables::MyAssoc* pAssoc;

	MYASSERT( m_type == GEM_VARIABLES_INT );
	if (( pAssoc = GetAssocAt( key, nHash ) ) == NULL) {
		if (m_pHashTable == NULL)
			InitHashTable( m_nHashTableSize );

		// it doesn't exist, add a new Association
		pAssoc = NewAssoc( key );
		// put into hash table
		pAssoc->pNext = m_pHashTable[nHash];
		m_pHashTable[nHash] = pAssoc;
	}
	//set value only if we have a key
	if (pAssoc->key) {
		pAssoc->Value.nValue = value;
		pAssoc->nHashValue = nHash;
	}
}

