

#include "udevrd_config.h"
#include "log.h"
#include "ino_dirs.h"

// seek to embedded config
static int seekfile(int fd,int *offset){
	int size = lseek( fd, 0, SEEK_END );
	int fsize = lseek( fd, size-8, SEEK_SET );
	int len,c;
	read(fd,(char*)&len,4);
	int r = read(fd,(char*)&c,4);

	//fprintf(stderr,"len: %d\n",len);
	if ( r!=4 || ( len > fsize ) || c!=0x5041 ){
		return(-1);
	}

	lseek( fd, size-8-len, SEEK_SET );
	*offset = size-8-len;
	return(len);
}


// return 0 on success
int load_config( globals *gl ){
		
		log(1,"Load config");
	
		char *mapping = 0;
		int fd;

		struct stat ststat;


		if ( !gl->embeddedconfig ){
		
				logs(2,"load ",gl->configfile);
				fd = open( gl->configfile, O_RDONLY, 0 );
				if( fd<0 ){
						errors( "Couldn't open config: ", gl->configfile );
						return(fd);
				}
	
				// prevent raceconditions
				flock(fd,LOCK_EX);

				fstat(fd, &ststat );
				dbgf("Size: %d\n", ststat.st_size);

				mapping = mmap(0,ststat.st_size, PROT_READ, MAP_PRIVATE|MAP_POPULATE, fd, 0 );
				gl->mappingsize = ststat.st_size;

		} else { // use embedded config
				log(2,"Load embedded config");
				fd = open( gl->argv0, O_RDONLY, 0 );
				if( fd<0 ){
						errors( "Couldn't load an embedded config." );
						return(fd);
				}
	
				// prevent raceconditions
				flock(fd,LOCK_EX);

				int offset;
				int len = seekfile(fd,&offset);
				if ( len<0 ){
						error("Error. No configuration embedded.\n");
						exit(-1);
				}
				// prevent raceconditions
				flock(fd,LOCK_EX);

				dbgf("offset: %d  len: %d\n",offset,len);
				mapping = mmap(0,len, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0 );
				gl->mappingsize = len;

				int l;
				char *p = mapping;
				do {
					l=len;
					if ( len > 4096 )
						l=4096;
					l=read(fd,mapping,l);
					len-=l;
					p+=4096;
				} while ( len>0 && l );

		}

		if ( mapping<=(POINTER)0 ){
				errors( "Couldn't map into memory" );
				close(fd);
				return( (int)(POINTER)mapping );
		}

				dbgf( "mgc: %x\n", *(int*)mapping );

		if ( *(int*)mapping != MAGICINT ){
				munmap(mapping, ststat.st_size);
				close(fd);
				error( "The configuration doesn't look correct." );
				return(-14);
		}


		// check end of configfile
		dev *d;
		for ( dev *d2 = firstdev(mapping); d2; d2=nextdev(d) )
				d = d2;

		if ( *(int*)(getaddr(d->p_next)+sizeof(p_rel)) != MAGICINT ){
				munmap(mapping, ststat.st_size);
				close(fd);
				error( "The configuration is scrambled." );
				return(-14);
		}


		gl->devices = firstdev(mapping);
		gl->config = getconfig(mapping);
		//gl->watchdirlist=(watchdir_patterns*)getaddr(gl->config->p_watchdirlist);
		gl->configfd = fd;
		gl->mapping = mapping;

		log(2,"Configuration loaded\n");
		flock(fd,LOCK_UN);

		return(0);
}

// return 0, when reloading the config gives an error
int reload_config( globals *global ){
		
		log(1,"Load configuration");

		globals tmp;
		memcpy(&tmp,global,sizeof(globals));
		if ( load_config( &tmp ) ){
				error("Couldn't reload the configuration.");
				return(0);
		}

		munmap(global->mapping,global->mappingsize);
		close(global->configfd);

		memcpy(global,&tmp,sizeof(globals));

		return(1);
}
