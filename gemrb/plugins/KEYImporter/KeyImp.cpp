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
 * $Header: /data/gemrb/cvs2svn/gemrb/gemrb/gemrb/plugins/KEYImporter/KeyImp.cpp,v 1.29 2003/12/18 15:05:22 balrog994 Exp $
 *
 */

#include "../../includes/win32def.h"
#include "KeyImp.h"
#include "../../includes/globals.h"
#include "../Core/FileStream.h"
#include "../Core/Interface.h"
#include "../Core/ArchiveImporter.h"
#include "../Core/AnimationMgr.h"
#ifndef WIN32
#include <unistd.h>
#endif

static char overridesubfolder[9]="override";
static char datasubfolder[5]="data";

KeyImp::KeyImp(void)
{
	if(core->CaseSensitive) {
		char path[_MAX_PATH];
		strcpy(path, core->GamePath);
		strcat(path, overridesubfolder);
		if(!dir_exists(path) ) {
			overridesubfolder[0]=toupper(overridesubfolder[0]);
		}
		strcpy(path, core->GamePath);
		strcat(path, datasubfolder);
		if(!dir_exists(path) ) {
			datasubfolder[0]=toupper(datasubfolder[0]);
		}
	}
}

KeyImp::~KeyImp(void)
{
	for(unsigned int i = 0; i < biffiles.size(); i++) {
		free(biffiles[i].name);
	}
}

bool KeyImp::LoadResFile(const char * resfile)
{
	char fn[_MAX_PATH] = {0};
#ifndef WIN32
	if(core->CaseSensitive) {
		ExtractFileFromPath(fn, resfile);
		char * newname = FindInDir(core->GamePath, fn);
		if(newname) {
			strcpy(fn, core->GamePath);
			strcat(fn, newname);
			free(newname);
		}
	}
	else
#endif
	strcpy(fn,resfile);
	printMessage("KEYImporter", "Opening ", WHITE);
	printf("%s...", fn);
	FileStream * f = new FileStream();
	if(!f->Open(fn)) {
		printStatus("ERROR", LIGHT_RED);
		printMessage("KEYImporter", "Cannot open Chitin.key\n", LIGHT_RED);
		textcolor(WHITE);
		delete(f);
		return false;
	}
	printStatus("OK", LIGHT_GREEN);
	printf("Checking file type...");
	char Signature[8];
	f->Read(Signature, 8);
	if(strncmp(Signature, "KEY V1  ", 8) != 0) {
		printStatus("ERROR", LIGHT_RED);
		printMessage("KEYImporter", "File has an Invalid Signature.\n", LIGHT_RED);
		textcolor(WHITE);
		delete(f);
		return false;
	}
	printStatus("OK", LIGHT_GREEN);
	printMessage("KEYImporter", "Reading Resources...\n", WHITE);
	unsigned long BifCount, ResCount, BifOffset, ResOffset;
	f->Read(&BifCount, 4);
	f->Read(&ResCount, 4);
	f->Read(&BifOffset, 4);
	f->Read(&ResOffset, 4);
	printf("BIF Files Count: %ld (Starting at %ld Bytes)\nRES Count: %ld (Starting at %ld Bytes)\n", BifCount, BifOffset, ResCount, ResOffset);
	f->Seek(BifOffset, GEM_STREAM_START);
	unsigned long BifLen, ASCIIZOffset;
	unsigned short ASCIIZLen;
	for(unsigned int i = 0; i < BifCount; i++) {
		BIFEntry be;
		f->Seek(BifOffset + (12*i), GEM_STREAM_START);
		f->Read(&BifLen, 4);
		f->Read(&ASCIIZOffset, 4);
		f->Read(&ASCIIZLen, 2);
		f->Read(&be.BIFLocator, 2);
		be.name = (char*)malloc(ASCIIZLen);
		f->Seek(ASCIIZOffset, GEM_STREAM_START);
		f->Read(be.name, ASCIIZLen);
#ifndef WIN32
		for(int p = 0; p < ASCIIZLen; p++) {
		  if(be.name[p] == '\\')
		    be.name[p] = '/';
		}
		if(core->CaseSensitive) {
			char fullPath[_MAX_PATH], tmpPath[_MAX_PATH] = {0}, fn[_MAX_PATH] = {0};
			strncpy( tmpPath , be.name, (strrchr(be.name, PathDelimiter)+1)-be.name);
			strcpy(fullPath, core->GamePath);
			strcat(fullPath, tmpPath);
			if(!dir_exists(fullPath) ) {
				tmpPath[0]=toupper(tmpPath[0]);
				be.name[0]=toupper(be.name[0]);
				strcpy(fullPath, core->GamePath);
				strcat(fullPath, tmpPath);
			}
			ExtractFileFromPath(fn, be.name);
			char * newname = FindInDir(fullPath, fn);
			if(newname) {
				strcpy(be.name, tmpPath);
				strcat(be.name, newname);
				free(newname);
			}
		}
#endif
		biffiles.push_back(be);
	}
	f->Seek(ResOffset, GEM_STREAM_START);
        resources.InitHashTable(ResCount);
	for(unsigned int i = 0; i < ResCount; i++) {
		RESEntry re;
		f->Read(re.ResRef, 8);
		f->Read(&re.Type, 2);
		f->Read(&re.ResLocator, 4);
		char *key;
		key=new char[8];
		for(int j=0;j<8;j++) key[j]=toupper(re.ResRef[j]);
		resources.SetAt(key, re.Type, re.ResLocator);
	}
	printMessage("KEYImporter", "Resources Loaded...", WHITE);
	printStatus("OK", LIGHT_GREEN);
	delete(f);
	return true;
}

#define SearchIn(BasePath, Path, ResRef, Type, foundMessage) \
{ \
	char p[_MAX_PATH], f[_MAX_PATH] = {0}; \
	strcpy(p, BasePath); \
	strcat(p, Path); \
	strcat(p, SPathDelimiter); \
	strncpy(f, ResRef, 8); \
	f[8] = 0; \
	strcat(f, core->TypeExt(Type)); \
	strlwr(f); \
	strcat(p, f); \
	FILE * exist = fopen(p, "rb"); \
	if(exist) { \
		printf(foundMessage); \
		fclose(exist); \
		FileStream * fs = new FileStream(); \
		if(!fs) return NULL; \
		fs->Open(p, true); \
		return fs; \
	} \
}

DataStream * KeyImp::GetResource(const char * resname, SClass_ID type)
{
	char path[_MAX_PATH];
	char BasePath[_MAX_PATH] = {0};
	char filename[_MAX_PATH] = {0};
	//Search it in the GemRB override Directory
	strcpy(path, "override"); //this shouldn't change
	strcat(path, SPathDelimiter);
	strcat(path, core->GameType);
	SearchIn(core->GemRBPath, path, resname, type, "[KEYImporter]: Found in GemRB Override...\n");
	SearchIn(core->GamePath, overridesubfolder, resname, type, "[KEYImporter]: Found in Override...\n");
	SearchIn(core->GamePath, datasubfolder, resname, type, "[KEYImporter]: Found in Local CD1 Folder...\n");
	printMessage("KEYImporter", "Searching for ", WHITE);
	printf("%.8s%s...", resname, core->TypeExt(type));
	unsigned long ResLocator;
	if(resources.Lookup(resname,type,ResLocator) ) {
		if(!core->IsAvailable(IE_BIF_CLASS_ID)) {
			printf("[ERROR]\nAn Archive Plug-in is not Available\n");
			return NULL;
		}
		int bifnum = (ResLocator & 0xFFF00000) >> 20;
		ArchiveImporter * ai = (ArchiveImporter*)core->GetInterface(IE_BIF_CLASS_ID);
		FILE * exist = NULL;
		if(exist == NULL) {
			strcpy(path, core->GamePath);
			strcat(path, biffiles[bifnum].name);
			exist = fopen(path, "rb");
			if(!exist) {
				path[0]=toupper(path[0]);
				exist = fopen(path, "rb");
			}
			if(!exist) {
				strcpy(path, core->GamePath);
				strncat(path, biffiles[bifnum].name, strlen(biffiles[bifnum].name)-4);
				strcat(path, ".cbf");
				exist = fopen(path, "rb");
				if(!exist) {
					path[0]=toupper(path[0]);
					exist = fopen(path, "rb");
				}
			}
		}
		if(exist == NULL) {
			if((biffiles[bifnum].BIFLocator & (1<<2)) != 0) {
				strcpy(BasePath, core->CD1);
			}
			else if((biffiles[bifnum].BIFLocator & (1<<3)) != 0) {
				strcpy(BasePath, core->CD2);
			}
			else if((biffiles[bifnum].BIFLocator & (1<<4)) != 0) {
				strcpy(BasePath, core->CD3);
			}
			else if((biffiles[bifnum].BIFLocator & (1<<5)) != 0) {
				strcpy(BasePath, core->CD4);
			}
			else if((biffiles[bifnum].BIFLocator & (1<<6)) != 0) {
				strcpy(BasePath, core->CD5);
			}
			else {
				printStatus("ERROR", LIGHT_RED);
				printf("Cannot find %s... Resource unavailable.\n", biffiles[bifnum].name);
				return NULL;
			}
			strcpy(path, BasePath);
			strcat(path, biffiles[bifnum].name);
			exist = fopen(path, "rb");
			if(exist == NULL) {
				//Trying CBF Extension
				strcpy(path, BasePath);
				strncat(path, biffiles[bifnum].name, strlen(biffiles[bifnum].name)-4);
				strcat(path, ".cbf");
				exist = fopen(path, "rb");
				if(!exist) {
					printStatus("ERROR", LIGHT_RED);
					printf("Cannot find %s\n", path);
					core->FreeInterface(ai);
					return NULL;
				}
			}
			
			fclose(exist);
		}
		else
			fclose(exist);
		ai->OpenArchive(path);
		DataStream * ret = ai->GetStream(ResLocator, type);
		core->FreeInterface(ai);
		if(ret) {
			strncpy(ret->filename, resname,8);
			ret->filename[8]=0;
			strcat(ret->filename, core->TypeExt(type));
		}
		return ret;
	}
	printStatus("ERROR", LIGHT_RED);
	return NULL;
}
void * KeyImp::GetFactoryResource(const char * resname, SClass_ID type, unsigned char mode)
{
	if(type != IE_BAM_CLASS_ID) {
		printf("\n");
		printMessage("KEYImporter", "", WHITE);
		printf("%s files are not supported.\n", core->TypeExt(type));
		return NULL;
	}
	int fobjindex;
	if((fobjindex = core->GetFactory()->IsLoaded(resname, type)) != -1) {
		//printf("\n");
		//printMessage("KEYImporter", "Factory Object Found!\n", WHITE);
		return core->GetFactory()->GetFactoryObject(fobjindex);
	}
	//printf("[KEYImporter]: No Factory Object Found, Loading...\n");
	char path[_MAX_PATH], filename[_MAX_PATH] = {0};
	//Search it in the GemRB override Directory
	strcpy(path, core->GemRBPath);
	strcat(path, "override"); //this shouldn't change!
	strcat(path, SPathDelimiter);
	strcat(path, core->GameType);
	strcat(path, SPathDelimiter);
	strncpy(filename, resname, 8);
	filename[8]=0;
	strcat(filename, core->TypeExt(type));
	strlwr(filename);
	strcat(path, filename);
	FILE * exist = fopen(path, "rb");
	if(exist) {
		//printf("[KEYImporter]: Found in GemRB Override...\n");
		fclose(exist);
		FileStream * fs = new FileStream();
		if(!fs)
			return NULL;
		fs->Open(path, true);
		return fs;
	}
	strcpy(path, core->GamePath);
	strcat(path, overridesubfolder);
	strcat(path, SPathDelimiter);
	strncat(path, resname, 8);
	strcat(path, core->TypeExt(type));
	exist = fopen(path, "rb");
	if(exist) {
		//printf("[KEYImporter]: Found in Override...\n");
		fclose(exist);
		AnimationMgr * ani = (AnimationMgr*)core->GetInterface(IE_BAM_CLASS_ID);
		if(!ani)
			return NULL;
		FileStream * fs = new FileStream();
		if(!fs)
			return NULL;
		fs->Open(path, true);
		ani->Open(fs, true);
		AnimationFactory * af = ani->GetAnimationFactory(resname, mode);
		core->FreeInterface(ani);
		core->GetFactory()->AddFactoryObject(af);
		return af;
	}
	/*printf("[KEYImporter]: Searching for %.8s%s...\n", resname, core->TypeExt(type));
	unsigned long ResLocator;
	if(resources.Lookup(resname, type, ResLocator) ) {
		int bifnum = (ResLocator & 0xFFF00000) >> 20;
		ArchiveImporter * ai = (ArchiveImporter*)core->GetInterface(IE_BIF_CLASS_ID);
		FILE * exist = NULL;
		if(exist == NULL) {
			strcpy(path, core->GamePath);
			strcat(path, biffiles[bifnum].name);
			exist = fopen(path, "rb");
		}
		if(exist == NULL) {
			if((biffiles[bifnum].BIFLocator & (1<<2)) != 0) {
				strcpy(path, core->CD1);
			}
			else if((biffiles[bifnum].BIFLocator & (1<<3)) != 0) {
				strcpy(path, core->CD2);
			}
			else if((biffiles[bifnum].BIFLocator & (1<<4)) != 0) {
				strcpy(path, core->CD3);
			}
			else if((biffiles[bifnum].BIFLocator & (1<<5)) != 0) {
				strcpy(path, core->CD4);
			}
			else if((biffiles[bifnum].BIFLocator & (1<<6)) != 0) {
				strcpy(path, core->CD5);
			}
			else {
				printf("[KEYImporter]: Error: Cannot find Bif file... Resource unavailable.\n");
				return NULL;
			}
			strcat(path, biffiles[bifnum].name);
			exist = fopen(path, "rb");
			if(exist == NULL) {
				printf("[KEYImporter]: Cannot find %s.\n", biffiles[bifnum].name);
				core->FreeInterface(ai);
				return NULL;
			}
			fclose(exist);
		}
		else
			fclose(exist);
		ai->OpenArchive(path);
		DataStream * ret = ai->GetStream(ResLocator, type);
		if(ret == NULL)
			printf("[NOT_FOUND]\n");
		core->FreeInterface(ai);
		strncpy(ret->filename, resname, 8);
		ret->filename[8] = 0;
		strcat(ret->filename, core->TypeExt(type));*/
	DataStream * ret = GetResource(resname, type);
	if(ret) {
		AnimationMgr * ani = (AnimationMgr*)core->GetInterface(IE_BAM_CLASS_ID);
		if(!ani)
			return NULL;
		ani->Open(ret, true);
		AnimationFactory * af = ani->GetAnimationFactory(resname, mode);
		core->FreeInterface(ani);
		core->GetFactory()->AddFactoryObject(af);
		return af;
	}
	return NULL;
}
