#ifndef IMAGE_RENAMING_DATA_H
#define IMAGE_RENAMING_DATA_H

#include <QString>
#include <QMap>

struct ImageRenamingData
{
	QString first; //old path
	QString second; //new path
#ifdef ENABLE_XATTR
	QMap<QString,QString> xattrs;
#endif
};

#endif // IMAGE_RENAMING_DATA_H
