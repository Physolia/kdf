/*
 * disks.cpp
 *
 * Copyright (c) 1998 Michael Kropfberger <michael.kropfberger@gmx.net>
 *
 * Requires the Qt widget libraries, available at no cost at
 * http://www.troll.no/
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <qfileinfo.h>
#include <QDir>

#include <kglobal.h>
#include <kdebug.h>

#include "disks.h"
#include "disks.moc"

/****************************************************/
/********************* DiskEntry ********************/
/****************************************************/

/**
  * Constructor
**/
void DiskEntry::init()
{
  device="";
  type="";
  mountedOn="";
  options="";
  size=0;
  used=0;
  avail=0;
  isMounted=FALSE;
  mntcmd="";
  umntcmd="";
  iconSetByUser=FALSE;
  icoName="";


 // BackgroundProcesses ****************************************

 sysProc = new KShellProcess(); Q_CHECK_PTR(sysProc);
 connect( sysProc, SIGNAL(receivedStdout(KProcess *, char *, int) ),
        this, SLOT (receivedSysStdErrOut(KProcess *, char *, int)) );
 connect( sysProc, SIGNAL(receivedStderr(KProcess *, char *, int) ),
        this, SLOT (receivedSysStdErrOut(KProcess *, char *, int)) );
 readingSysStdErrOut=FALSE;


}

DiskEntry::DiskEntry(QObject *parent, const char *name)
 : QObject (parent, name)
{
  init();
}

DiskEntry::DiskEntry(const QString & deviceName, QObject *parent, const char *name)
 : QObject (parent, name)
{
  init();

  setDeviceName(deviceName);
}
DiskEntry::~DiskEntry()
{
  disconnect(this);
  delete sysProc;
}

int DiskEntry::toggleMount()
{
  if (!mounted())
      return mount();
  else
      return umount();
}

int DiskEntry::mount()
{
  QString cmdS=mntcmd;
  if (cmdS.isEmpty()) // generate default mount cmd
    if (getuid()!=0 ) // user mountable
      {
      cmdS="mount %d";
      }
	else  // root mounts with all params/options
      {
      // FreeBSD's mount(8) is picky: -o _must_ go before
      // the device and mountpoint.
      cmdS=QString::fromLatin1("mount -t%t -o%o %d %m");
      }

  cmdS.replace(QString::fromLatin1("%d"),deviceName());
  cmdS.replace(QString::fromLatin1("%m"),mountPoint());
  cmdS.replace(QString::fromLatin1("%t"),fsType());
  cmdS.replace(QString::fromLatin1("%o"),mountOptions());

  kDebug() << "mount-cmd: [" << cmdS << "]" << endl;
  int e=sysCall(cmdS);
  if (!e) setMounted(TRUE);
  kDebug() << "mount-cmd: e=" << e << endl;
  return e;
}

int DiskEntry::umount()
{
  kDebug() << "umounting" << endl;
  QString cmdS=umntcmd;
  if (cmdS.isEmpty()) // generate default umount cmd
      cmdS="umount %d";

  cmdS.replace(QString::fromLatin1("%d"),deviceName());
  cmdS.replace(QString::fromLatin1("%m"),mountPoint());

  kDebug() << "umount-cmd: [" << cmdS << "]" << endl;
  int e=sysCall(cmdS);
  if (!e) setMounted(FALSE);
  kDebug() << "umount-cmd: e=" << e << endl;

  return e;
}

int DiskEntry::remount()
{
  if (mntcmd.isEmpty() && umntcmd.isEmpty() // default mount/umount commands
      && (getuid()==0)) // you are root
    {
    QString oldOpt=options;
    if (options.isEmpty())
       options="remount";
    else
       options+=",remount";
    int e=mount();
    options=oldOpt;
    return e;
   } else {
    if (int e=umount())
      return mount();
   else return e;
  }
}

void DiskEntry::setMountCommand(const QString & mnt)
{
  mntcmd=mnt;
}

void DiskEntry::setUmountCommand(const QString & umnt)
{
  umntcmd=umnt;
}

void DiskEntry::setIconName(const QString & iconName)
{
  iconSetByUser=TRUE;
  icoName=iconName;
  if (icoName.right(6) == "_mount")
     icoName.truncate(icoName.length()-6);
  else if (icoName.right(8) == "_unmount")
     icoName.truncate(icoName.length()-8);

  emit iconNameChanged();
}

QString DiskEntry::iconName()
{
  QString iconName=icoName;
  if (iconSetByUser) {
    mounted() ? iconName+="_mount" : iconName+="_unmount";
   return iconName;
  } else
   return guessIconName();
}

QString DiskEntry::guessIconName()
{
  QString iconName;
    // try to be intelligent
    if (mountPoint().contains("cdrom",Qt::CaseInsensitive)) iconName+="cdrom";
    else if (deviceName().contains("cdrom",Qt::CaseInsensitive)) iconName+="cdrom";
    else if (mountPoint().contains("writer",Qt::CaseInsensitive)) iconName+="cdwriter";
    else if (deviceName().contains("writer",Qt::CaseInsensitive)) iconName+="cdwriter";
    else if (mountPoint().contains("mo",Qt::CaseInsensitive)) iconName+="mo";
    else if (deviceName().contains("mo",Qt::CaseInsensitive)) iconName+="mo";
    else if (deviceName().contains("fd",Qt::CaseInsensitive)) {
            if (deviceName().contains("360",Qt::CaseInsensitive)) iconName+="5floppy";
            if (deviceName().contains("1200",Qt::CaseInsensitive)) iconName+="5floppy";
            else iconName+="3floppy";
	 }
    else if (mountPoint().contains("floppy",Qt::CaseInsensitive)) iconName+="3floppy";
    else if (mountPoint().contains("zip",Qt::CaseInsensitive)) iconName+="zip";
    else if (fsType().contains("nfs",Qt::CaseInsensitive)) iconName+="nfs";
    else iconName+="hdd";
    mounted() ? iconName+="_mount" : iconName+="_unmount";
//    if ( !mountOptions().contains("user",Qt::CaseInsensitive) )
//      iconName.prepend("root_"); // special root icon, normal user can't mount

    //debug("device %s is %s",deviceName().latin1(),iconName.latin1());

    //emit iconNameChanged();
  return iconName;
}


/***************************************************************************
  * starts a command on the underlying system via /bin/sh
**/
int DiskEntry::sysCall(const QString & command)
{
  if (readingSysStdErrOut || sysProc->isRunning() )  return -1;

  sysStringErrOut=i18n("Called: %1\n\n", command); // put the called command on ErrOut
  sysProc->clearArguments();
  (*sysProc) << command;
    if (!sysProc->start( KProcess::Block, KProcess::AllOutput ))
     kFatal() << i18n("could not execute %1", command.local8Bit().data()) << endl;

  if (sysProc->exitStatus()!=0) emit sysCallError(this, sysProc->exitStatus());

  return (sysProc->exitStatus());
}


/***************************************************************************
  * is called, when the Sys-command writes on StdOut or StdErr
**/
void DiskEntry::receivedSysStdErrOut(KProcess *, char *data, int len)
{
  QString tmp = QString::fromLocal8Bit(data, len);
  sysStringErrOut.append(tmp);
}

float DiskEntry::percentFull() const
{
   if (size != 0) {
      return 100 - ( ((float)avail / (float)size) * 100 );
   } else {
      return -1;
   }
}

void DiskEntry::setDeviceName(const QString & deviceName)
{
 device=deviceName;
 emit deviceNameChanged();
}

QString DiskEntry::deviceRealName() const
{
 QFileInfo inf( device );
 QDir dir( inf.absolutePath() );
 QString relPath = inf.fileName();
 if ( inf.isSymLink() ) {
  QString link = inf.readLink();
  if ( link.startsWith( "/" ) )
    return link;
  relPath = link;
 }
 return dir.canonicalPath() + "/" + relPath;
}

void DiskEntry::setMountPoint(const QString & mountPoint)
{
  mountedOn=mountPoint;
 emit mountPointChanged();
}

QString DiskEntry::realMountPoint() const
{
 QDir dir( mountedOn );
 return dir.canonicalPath();
}

void DiskEntry::setMountOptions(const QString & mountOptions)
{
 options=mountOptions;
 emit mountOptionsChanged();
}

void DiskEntry::setFsType(const QString & fsType)
{
  type=fsType;
  emit fsTypeChanged();
}

void DiskEntry::setMounted(bool nowMounted)
{
  isMounted=nowMounted;
  emit mountedChanged();
}

void DiskEntry::setKBSize(int kb_size)
{
  size=kb_size;
  emit kBSizeChanged();
}

void DiskEntry::setKBUsed(int kb_used)
{
  used=kb_used;
  if ( size < (used+avail) ) {  //adjust kBAvail
     kWarning() << "device " << device << ": kBAvail(" << avail << ")+*kBUsed(" << used << ") exceeds kBSize(" << size << ")" << endl;
     setKBAvail(size-used);
  }
  emit kBUsedChanged();
}

void DiskEntry::setKBAvail(int kb_avail)
{
  avail=kb_avail;
  if ( size < (used+avail) ) {  //adjust kBUsed
     kWarning() << "device " << device << ": *kBAvail(" << avail << ")+kBUsed(" << used << ") exceeds kBSize(" << size << ")" << endl;
     setKBUsed(size-avail);
  }
  emit kBAvailChanged();
}


