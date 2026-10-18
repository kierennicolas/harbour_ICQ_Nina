/*
    Kopete Oscar Protocol
    Oscar Backend Setting Storage

    Copyright (c) 2005 Matt Rogers <mattr@kde.org>

    Kopete (c) 2002-2005 by the Kopete developers <kopete-devel@kde.org>

    *************************************************************************
    *                                                                       *
    * This library is free software; you can redistribute it and/or         *
    * modify it under the terms of the GNU Lesser General Public            *
    * License as published by the Free Software Foundation; either          *
    * version 2 of the License, or (at your option) any later version.      *
    *                                                                       *
    *************************************************************************
*/
#ifndef OSCARSETTINGS_H
#define OSCARSETTINGS_H

#include <QString>
#include "liboscar_export.h"

namespace Oscar
{

/**
* This class is used to keep track of various persistant settings that the backend will always
* need to get from the frontend. This is the interface and storage class that will handle the
* settings.
* @author Matt Rogers
*/
class LIBOSCAR_EXPORT Settings
{
public:
	Settings();
	~Settings();

	/* Web awareness settings */
	void setWebAware( bool webAware );
	bool webAware() const;
	
	/* Authorization settings */
	void setRequireAuth( bool require );
	bool requireAuth() const;
	
	/* Hide IP Settings */
	void setHideIP( bool hide );
	bool hideIP() const;
	
	/* proxy Settings */
	void setFileProxy( bool proxy );
	bool fileProxy() const;
	
	/* port range settings */
	void setFirstPort( int port );
	int firstPort() const;
	void setLastPort( int port );
	int lastPort() const;
	
	/* timeout in seconds */
	void setTimeout( int time );
	int timeout() const;

	/* harbour_icq_nina: sign-on variants, for servers (such as revival
	 * services) that only accept some of the historical forms. */

	/* BUCP: send MD5(key + MD5(password) + AIM string) with TLV 0x4C, as
	 * libpurple and AIM 5.x do, instead of MD5(key + password + AIM string). */
	void setStrongPasswordHash( bool strong );
	bool strongPasswordHash() const;

	/* BUCP: add the empty TLVs 0x4B and 0x5A to the key request (17/06), as
	 * libpurple does. */
	void setExtendedKeyRequest( bool extended );
	bool extendedKeyRequest() const;

	/* Multiple-session flag (TLV 0x4A) in the login request: -1 sends it only
	 * for AIM (Kopete's behaviour), otherwise the value to send for both AIM
	 * and ICQ (0x01 = join other sessions, 0x03 = single session). */
	void setMultiConnection( int flag );
	int multiConnection() const;

	/* Rendezvous (file transfer) proxy. Kopete hard-coded AOL's
	 * ars.oscar.aol.com:5190; revival services run their own. */
	void setFileProxyHost( const QString &host );
	QString fileProxyHost() const;
	void setFileProxyPort( int port );
	int fileProxyPort() const;

private:
	
	bool m_webAware;
	bool m_requireAuth;
	bool m_hideIP;
	bool m_fileProxy;
	int  m_firstPort;
	int  m_lastPort;
	int  m_timeout;
	bool m_strongPasswordHash;
	bool m_extendedKeyRequest;
	int  m_multiConnection;
	QString m_fileProxyHost;
	int  m_fileProxyPort;
};

}

#endif

