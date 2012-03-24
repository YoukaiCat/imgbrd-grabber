#include "image.h"
#include "functions.h"



Image::Image(QMap<QString, QString> details, int timezonedecay, Page* parent)
{
	m_site = parent->website();
	m_url = details.contains("file_url") ? (details["file_url"].startsWith("/") ? "http://"+m_site+details["file_url"] : details["file_url"]) : "";
	m_md5 = details.contains("md5") ? details["md5"] : "";
	m_author = details.contains("author") ? details["author"] : "";
	m_status = details.contains("status") ? details["status"] : "";
	m_rating = details.contains("rating") ? details["rating"] : "";
	QStringMap assoc;
		assoc["s"] = tr("Safe");
		assoc["q"] = tr("Questionable");
		assoc["e"] = tr("Explicit");
	if (assoc.contains(m_rating))
	{ m_rating = assoc[m_rating]; }
	m_source = details.contains("source") ? details["source"] : "";
	m_tags = QList<Tag>();
	if (details.contains("tags"))
	{
		QStringList t = details["tags"].split(" ");
		for (int i = 0; i < t.count(); i++)
		{
			QString tg = t.at(i);
			tg.replace("&amp;", "&");
			m_tags.append(Tag(tg));
		}
	}
	m_id = details.contains("id") ? details["id"].toInt() : 0;
	m_score = details.contains("score") ? details["score"].toInt() : 0;
	m_hasScore = details.contains("score");
	m_parentId = details.contains("parent_id") ? details["parent_id"].toInt() : 0;
	m_fileSize = details.contains("file_size") ? details["file_size"].toInt() : 0;
	m_authorId = details.contains("creator_id") ? details["creator_id"].toInt() : 0;
	m_createdAt = QDateTime();
	if (details.contains("created_at"))
	{
		if (details["created_at"].toInt() != 0)
		{ m_createdAt.setTime_t(details["created_at"].toInt()); }
		else
		{ m_createdAt = qDateTimeFromString(details["created_at"], timezonedecay); }
	}
	m_hasChildren = details.contains("has_children") ? details["has_children"] == "true" : false;
	m_hasNote = details.contains("has_note") ? details["has_note"] == "true" : false;
	m_hasComments = details.contains("has_comments") ? details["has_comments"] == "true" : false;
	m_pageUrl = details.contains("page_url") ? QUrl(details["page_url"]) : QUrl();
	m_fileUrl = details.contains("file_url") ? QUrl(details["file_url"].startsWith("/") ? "http://"+m_site+details["file_url"] : details["file_url"]) : QUrl();
	m_sampleUrl = details.contains("sample_url") ? QUrl(details["sample_url"].startsWith("/") ? "http://"+m_site+details["sample_url"] : details["sample_url"]) : QUrl();
	m_previewUrl = details.contains("preview_url") ? QUrl(details["preview_url"].startsWith("/") ? "http://"+m_site+details["preview_url"] : details["preview_url"]) : QUrl();
	m_size = QSize(details.contains("width") ? details["width"].toInt() : 0, details.contains("height") ? details["height"].toInt() : 0);
	m_parent = parent;

	m_previewTry = 0;
	m_loadPreviewExists = false;
	m_loadTagsExists = false;
	m_loadImageExists = false;
	m_pools = QList<Pool*>();
}
Image::~Image()
{ }

void Image::loadPreview()
{
	QNetworkAccessManager *manager = new QNetworkAccessManager(this);
		/*QNetworkDiskCache *diskCache = new QNetworkDiskCache(this);
		diskCache->setCacheDirectory(QDesktopServices::storageLocation(QDesktopServices::CacheLocation));
		manager->setCache(diskCache);*/

	connect(manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(parsePreview(QNetworkReply*)));
	connect(manager, SIGNAL(finished(QNetworkReply*)), manager, SLOT(deleteLater()));
	QNetworkRequest r(m_previewUrl);
		//r.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
		r.setRawHeader("Referer", m_previewUrl.toString().toAscii());

	m_previewTry++;
	m_loadPreviewExists = true;
	m_loadPreview = manager->get(r);
}
void Image::abortPreview()
{
	if (m_loadPreviewExists)
	{
		if (m_loadPreview->isRunning())
		{ m_loadPreview->abort(); }
	}
}
void Image::parsePreview(QNetworkReply* r)
{
	// Check redirection
	QUrl redir = r->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
	if (!redir.isEmpty())
	{
		m_previewUrl = redir;
		loadPreview();
		return;
	}

	// Load preview from raw result
	m_imagePreview.loadFromData(r->readAll());
	r->deleteLater();
	m_loadPreviewExists = false;

	// If nothing has been received
	if (m_imagePreview.isNull() && m_previewTry <= 3)
	{
		log(tr("<b>Attention :</b> %1").arg(tr("une des miniatures est vide (<a href=\"%1\">%1</a>). Nouvel essai (%2/%3)...").arg(m_previewUrl.toString()).arg(m_previewTry).arg(3)));
		loadPreview();
	}
	else
	{ emit finishedLoadingPreview(this); }
}

void Image::loadTags()
{
	QNetworkAccessManager *manager = new QNetworkAccessManager(this);
		/*QNetworkDiskCache *diskCache = new QNetworkDiskCache(this);
		diskCache->setCacheDirectory(QDesktopServices::storageLocation(QDesktopServices::CacheLocation));
		manager->setCache(diskCache);*/

	connect(manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(parseTags(QNetworkReply*)));
	QNetworkRequest r(m_pageUrl);
		//r.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
		r.setRawHeader("Referer", m_pageUrl.toString().toAscii());

	m_loadTags = manager->get(r);
	m_loadTagsExists = true;
}
void Image::abortTags()
{
	if (m_loadTagsExists)
	{
		if (m_loadTags->isRunning())
		{ m_loadTags->abort(); }
	}
}
void Image::parseTags(QNetworkReply* r)
{
	// Check redirection
	QUrl redir = r->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
	if (!redir.isEmpty())
	{
		m_pageUrl = redir;
		loadTags();
		return;
	}
	QString source = QString::fromUtf8(r->readAll());

	// Pools
	if (m_parent->site().contains("Regex/Pools"))
	{
		m_pools.clear();
		QRegExp rx(m_parent->site().value("Regex/Pools"));
		rx.setMinimal(true);
		int pos = 0;
		while ((pos = rx.indexIn(source, pos)) != -1)
		{
			pos += rx.matchedLength();
			QString previous = rx.cap(1), id = rx.cap(2), name = rx.cap(3), next = rx.cap(4);
			m_pools.append(new Pool(id.toInt(), name, m_id, next.toInt(), previous.toInt()));
		}
	}

	// Tags
	if (m_parent->site().contains("Regex/Tags"))
	{
		QRegExp rx(m_parent->site().value("Regex/Tags"));
		rx.setMinimal(true);
		int pos = 0;
		QList<Tag> tgs;
		while ((pos = rx.indexIn(source, pos)) != -1)
		{
			pos += rx.matchedLength();
			QString type = rx.cap(1), tag = rx.cap(2).replace(" ", "_").replace("&amp;", "&");
			int count = rx.cap(3).toInt();
			tgs.append(Tag(tag, type, count));
		}
		if (!tgs.isEmpty())
		{ m_tags = tgs; }
	}

	emit finishedLoadingTags(this);
}

QString Image::filter(QStringList filters)
{
	QStringList types = QStringList() << "rating" << "source" << "id" << "width" << "height" << "score" << "mpixels";
	bool invert;
	QString filter, type;
	for (int i = 0; i < filters.count(); i++)
	{
		invert = false;
		filter = filters.at(i);
		if (filter.startsWith('-'))
		{
			filter = filter.right(filter.length()-1);
			invert = true;
		}
		if (filter.contains(":"))
		{
			type = filter.section(':', 0, 0).toLower();
			filter = filter.section(':', 1).toLower();
			if (!types.contains(type))
			{ return QObject::tr("unknown type \"%1\" (available types: \"%2\")").arg(filter, types.join("\", \"")); }
			if (type == "rating")
			{
				bool cond = m_rating.toLower().startsWith(filter.left(1));
				if (!cond && !invert)
				{ return QObject::tr("image is not \"%1\"").arg(filter); }
				if (cond && invert)
				{ return QObject::tr("image is \"%1\"").arg(filter); }
			}
			else if (type == "source")
			{
				QRegExp rx = QRegExp(filter+"*", Qt::CaseInsensitive, QRegExp::Wildcard);
				bool cond = rx.exactMatch(m_source);
				if (!cond && !invert)
				{ return QObject::tr("image's source does not starts with \"%1\"").arg(filter); }
				if (cond && invert)
				{ return QObject::tr("image's source starts with \"%1\"").arg(filter); }
			}
			else if (type == "id" || type == "width" || type == "height" || type == "score" || type == "mpixels")
			{
				int input = 0;
				if (type == "id")		{ input = m_id;								}
				if (type == "width")	{ input = m_size.width();					}
				if (type == "height")	{ input = m_size.height();					}
				if (type == "score")	{ input = m_score;							}
				if (type == "mpixels")	{ input = m_size.width()*m_size.height();	}

				bool cond;
				if (filter.startsWith("..") || filter.startsWith("<="))
				{ cond = input <= filter.right(filter.size()-2).toInt(); }
				else if (filter.endsWith  ("..") || filter.startsWith(">="))
				{ cond = input >= filter.right(filter.size()-2).toInt(); }
				else if (filter.startsWith("<"))
				{ cond = input < filter.right(filter.size()-1).toInt(); }
				else if (filter.startsWith(">"))
				{ cond = input > filter.right(filter.size()-1).toInt(); }
				else if (filter.contains(".."))
				{ cond = input >= filter.left(filter.indexOf("..")).toInt() && input <= filter.right(filter.size()-filter.indexOf("..")-2).toInt();	}
				else
				{ cond = input == filter.toInt(); }

				if (!cond && !invert)
				{ return QObject::tr("image's %1 does not match").arg(type); }
				if (cond && invert)
				{ return QObject::tr("image's %1 match").arg(type); }
			}
		}
		else if (!filter.isEmpty())
		{
			bool cond = false;
			for (int t = 0; t < m_tags.count(); t++)
			{
				if (m_tags[t].text().toLower() == filter.toLower())
				{ cond = true; break; }
			}
			if (!cond && !invert)
			{ return QObject::tr("image does not contains \"%1\"").arg(filter); }
			if (cond && invert)
			{ return QObject::tr("image contains \"%1\"").arg(filter); }
		}
	}
	return QString();
}



QString analyse(QStringList tokens, QString text, QStringList tags)
{
	QString ret = text;
	QRegExp reg = QRegExp("\\<([^>]+)\\>");
	int pos = 0;
	while ((pos = reg.indexIn(text, pos)) != -1)
	{
		QString cap = reg.cap(1);
		if (!cap.isEmpty())
		{
			cap += QString(">").repeated(cap.count('<')-cap.count('>'));
			ret.replace("<"+cap+">", analyse(tokens, cap, tags));
		}
		pos += reg.matchedLength()+cap.count('<')-cap.count('>');
	}
	QString r = ret;
	for (int i = 0; i < tokens.size(); i++)
	{ r.replace("%"+tokens.at(i)+"%", ""); }
	reg = QRegExp("\"([^\"]+)\"");
	pos = 0;
	while ((pos = reg.indexIn(text, pos)) != -1)
	{
		if (!reg.cap(1).isEmpty() && tags.contains(reg.cap(1)))
		{ ret.replace(reg.cap(0), reg.cap(1)); }
		pos += reg.matchedLength();
	}
	return r.contains("%") || ret.contains("\"") ? "" : ret;
}

QString Image::path(QString fn, QString pth)
{
	QSettings settings(savePath("settings.ini"), QSettings::IniFormat);
	settings.beginGroup("Save");
	if (fn.isEmpty())
	{ fn = settings.value("filename").toString(); }
	if (pth.isEmpty())
	{ pth = settings.value("path").toString(); }

	QMap<QString,QPair<QString,QString> > replaces = QMap<QString,QPair<QString,QString> >();
	QStringList copyrights;
	QString cop;
	bool found;
	QMap<QString,QStringList> custom = QMap<QString,QStringList>(), scustom = getCustoms();
	QMap<QString,QStringList> details;
	QStringList ignore = loadIgnored();
	for (int i = 0; i < m_tags.size(); i++)
	{
		QString t = m_tags[i].text();
		for (int r = 0; r < scustom.size(); r++)
		{
			if (!custom.contains(scustom.keys().at(r)))
			{ custom.insert(scustom.keys().at(r), QStringList()); }
			if (scustom.values().at(r).contains(t))
			{ custom[scustom.keys().at(r)].append(t); }
		}
		details["allos"].append(t);
		details[ignore.contains(m_tags[i].text(), Qt::CaseInsensitive) ? "generals" : m_tags[i].type()+"s"].append(t);
		details["alls"].append(t);
	}
	if (settings.value("copyright_useshorter", true).toBool())
	{
		for (int i = 0; i < details["copyrights"].size(); i++)
		{
			found = false;
			cop = details["copyrights"].at(i);
			for (int r = 0; r < copyrights.size(); r++)
			{
				if (copyrights.at(r).left(cop.size()) == cop.left(copyrights.at(r).size()))
				{
					if (cop.size() < copyrights.at(r).size())
					{ copyrights[r] = cop; }
					found = true;
				}
			}
			if (!found)
			{ copyrights.append(cop); }
		}
	}
	else
	{ copyrights = details["copyrights"]; }
	QStringList search = m_parent->search();

	QString ext = m_url.section('.', -1);
	if (ext.length() > 5)
	{ ext = "jpg"; }

	replaces.insert("%ext%", QPair<QString,QString>(ext, "jpg"));
	replaces.insert("%filename%", QPair<QString,QString>(m_url.section('/', -1).section('.', 0, -2), ""));
	replaces.insert("%website%", QPair<QString,QString>(m_site, ""));
	replaces.insert("%md5%", QPair<QString,QString>(m_md5, ""));
	replaces.insert("%id%", QPair<QString,QString>(QString::number(m_id), "0"));
	for (int i = 0; i < search.size(); i++)
	{ replaces.insert("%search_"+QString::number(i+1)+"%", QPair<QString,QString>(search[i], "")); }
	replaces.insert("%search%", QPair<QString,QString>(search.join(settings.value("separator").toString()), ""));
	replaces.insert("%artist%", QPair<QString,QString>(details["artists"].count() > 0 ? (settings.value("artist_useall").toBool() || details["artists"].count() == 1 ? details["artists"].join(settings.value("artist_sep").toString()) : settings.value("artist_value").toString()) : "", settings.value("artist_empty").toString()));
	replaces.insert("%copyright%", QPair<QString,QString>(details["copyrights"].count() > 0 ? (settings.value("copyright_useall").toBool() || details["copyrights"].count() == 1 ? details["copyrights"].join(settings.value("copyright_sep").toString()) : settings.value("copyright_value").toString()) : "", settings.value("copyright_empty").toString()));
	replaces.insert("%character%", QPair<QString,QString>(details["characters"].count() > 0 ? (settings.value("character_useall").toBool() || details["characters"].count() == 1 ? details["characters"].join(settings.value("character_sep").toString()) : settings.value("character_value").toString()) : "", settings.value("character_empty").toString()));
	replaces.insert("%model%", QPair<QString,QString>(details["models"].count() > 0 ? (settings.value("model_useall").toBool() || details["models"].count() == 1 ? details["models"].join(settings.value("model_sep").toString()) : settings.value("model_value").toString()) : "", settings.value("model_empty").toString()));
	replaces.insert("%rating%", QPair<QString,QString>(m_rating, ""));
	replaces.insert("%height%", QPair<QString,QString>(QString::number(m_size.height()), "0"));
	replaces.insert("%width%", QPair<QString,QString>(QString::number(m_size.width()), "0"));
	for (int i = 0; i < custom.size(); i++)
	{ replaces.insert("%"+custom.keys().at(i)+"%", QPair<QString,QString>(custom.values().at(i).join(settings.value("separator").toString()), "")); }
	replaces.insert("%general%", QPair<QString,QString>(details["generals"].join(settings.value("separator").toString()), ""));
	replaces.insert("%allo%", QPair<QString,QString>(details["allos"].join(" "), ""));
	replaces.insert("%all%", QPair<QString,QString>(details["alls"].join(" "), ""));

	// Filename
	QString filename = fn;
	QMap<QString,QString> filenames = getFilenames();
	for (int i = 0; i < filenames.size(); i++)
	{
		QString cond = filenames.keys().at(i);
		if (cond.startsWith("%") && cond.endsWith("%"))
		{
			if (replaces.contains(cond))
			{
				if (!replaces.value(cond).first.isEmpty())
				{ filename = filenames.value(cond); }
			}
		}
		else if (details["alls"].contains(cond))
		{ filename = filenames.value(cond); }
	}

	// We get path and remove useless slashes from filename
	pth.replace("\\", "/");
	filename.replace("\\", "/");
	if (filename.left(1) == "/")	{ filename = filename.right(filename.length()-1);	}
	if (pth.right(1) == "/")		{ pth = pth.left(pth.length()-1);					}

	// Conditionals
	QStringList c = custom.keys();
	for (int i = 0; i < 10; i++)
	{ c.append("search_"+QString::number(i)); }
	QStringList tokens = QStringList() << "artist" << "general" << "copyright" << "character" << "model" << "model|artist" << "filename" << "rating" << "md5" << "website" << "ext" << "all" << "id" << "search" << "allo" << c;
	filename = analyse(tokens, filename, details["allos"]);

	// No duplicates in %all%
	QStringList rem = (filename.contains("%artist%") ? details["artists"] : QStringList()) +
		(filename.contains("%copyright%") ? copyrights : QStringList()) +
		(filename.contains("%character%") ? details["characters"] : QStringList()) +
		(filename.contains("%model%") ? details["models"] : QStringList()) +
		(filename.contains("%general%") ? details["generals"] : QStringList());
	QStringList l = details["alls"];
	for (int i = 0; i < rem.size(); i++)
	{ l.removeAll(rem.at(i)); }
	replaces.insert("%all%", QPair<QString,QString>(l.join(settings.value("separator").toString()), ""));

	// We replace everithing
	for (int i = 0; i < replaces.size(); i++)
	{
		QString res = replaces.values().at(i).first.isEmpty() ? replaces.values().at(i).second : replaces.values().at(i).first;
		res = res.replace("\\", "_").replace("%", "_").replace("/", "_").replace(":", "_").replace("|", "_").replace("*", "_").replace("?", "_").replace("\"", "_").replace("<", "_").replace(">", "_").replace("__", "_").replace("__", "_").replace("__", "_").trimmed();
		if (!settings.value("replaceblanks", false).toBool())
		{ res.replace("_", " "); }

		// We only cut the name if it is not a folder
		if (!filename.right(filename.length()-filename.indexOf(replaces.keys().at(i))).contains("/"))
		{ filename.replace(replaces.keys().at(i), res.left(259-pth.length()-1-filename.length())); }
		else
		{ filename.replace(replaces.keys().at(i), res); }
	}

	// We remove empty dir names
	while (filename.indexOf("//") >= 0)
	{ filename.replace("//", "/"); }

	// Max filename size option
	if (filename.length() > settings.value("limit").toInt() && settings.value("limit").toInt() > 0)
	{ filename = filename.left(filename.length()-ext.length()-1).left(settings.value("limit").toInt()-ext.length()-1) + filename.right(ext.length()+1); }

	return QDir::toNativeSeparators(filename);
}

void Image::loadImage()
{
	QNetworkAccessManager *m = new QNetworkAccessManager(this);
	QNetworkRequest request(m_url);
		request.setRawHeader("Referer", m_url.toAscii());

	m_loadImage = m->get(request);
	//m_timer.start();
	connect(m_loadImage, SIGNAL(downloadProgress(qint64, qint64)), this, SLOT(downloadProgressImageS(qint64, qint64)));
	connect(m_loadImage, SIGNAL(finished()), this, SLOT(finishedImageS()));
	m_loadImageExists = true;
}
void Image::finishedImageS()
{ emit finishedImage(this); }
void Image::downloadProgressImageS(qint64 v1, qint64 v2)
{
	if (v2 > 0/* && (v1 == v2 || m_timer.elapsed() > 500)*/)
	{
		//m_timer.restart();
		emit downloadProgressImage(this, v1, v2);
	}
}
void Image::abortImage()
{
	if (m_loadImageExists)
	{
		if (m_loadImage->isRunning())
		{ m_loadImage->abort(); }
	}
}

int Image::value()
{
	int pixels;
	if (!m_size.isEmpty())
	{ pixels = m_size.width()*m_size.height(); }
	else
	{
		pixels = 1200*900;
		QStringList tags;
		for (int t = 0; t < m_tags.size(); t++)
		{ tags.append(m_tags[t].text().toLower()); }
		if (tags.contains("incredibly_absurdres"))	{ pixels = 10000*10000; }
		else if (tags.contains("absurdres"))		{ pixels = 3200*2400; }
		else if (tags.contains("highres"))			{ pixels = 1600*1200; }
		else if (tags.contains("lowres"))			{ pixels = 500*500; }
	}
	return pixels;
}

QStringList Image::blacklisted(QStringList blacklistedtags)
{
	QStringList detected;
	QRegExp reg;
	reg.setCaseSensitivity(Qt::CaseInsensitive);
	reg.setPatternSyntax(QRegExp::Wildcard);
	for (int i = 0; i < blacklistedtags.size(); i++)
	{
		for (int t = 0; t < m_tags.count(); t++)
		{
			reg.setPattern(blacklistedtags.at(i));
			if (reg.exactMatch(m_tags[t].text()))
			{ detected.append(m_tags[t].text()); }
		}
	}
	return detected;
}



QString			Image::url()			{ return m_url;				}
QString			Image::md5()			{ return m_md5;				}
QString			Image::author()			{ return m_author;			}
QString			Image::status()			{ return m_status;			}
QString			Image::rating()			{ return m_rating;			}
QString			Image::source()			{ return m_source;			}
QString			Image::site()			{ return m_site;			}
QList<Tag>		Image::tags()			{ return m_tags;			}
QList<Pool*>	Image::pools()			{ return m_pools;			}
int				Image::id()				{ return m_id;				}
int				Image::score()			{ return m_score;			}
int				Image::parentId()		{ return m_parentId;		}
int				Image::fileSize()		{ return m_fileSize;		}
int				Image::width()			{ return m_size.width();	}
int				Image::height()			{ return m_size.height();	}
int				Image::authorId()		{ return m_authorId;		}
QDateTime		Image::createdAt()		{ return m_createdAt;		}
bool			Image::hasChildren()	{ return m_hasChildren;		}
bool			Image::hasNote()		{ return m_hasNote;			}
bool			Image::hasComments()	{ return m_hasComments;		}
bool			Image::hasScore()		{ return m_hasScore;		}
QUrl			Image::fileUrl()		{ return m_fileUrl;			}
QUrl			Image::sampleUrl()		{ return m_sampleUrl;		}
QUrl			Image::previewUrl()		{ return m_previewUrl;		}
QUrl			Image::pageUrl()		{ return m_pageUrl;			}
QSize			Image::size()			{ return m_size;			}
QPixmap			Image::previewImage()	{ return m_imagePreview;	}
Page			*Image::page()			{ return m_parent;			}
QByteArray		Image::data()			{ return m_data;			}
QNetworkReply	*Image::imageReply()	{ return m_loadImage;		}

void	Image::setUrl(QString u)		{ m_url = u;	}
void	Image::setData(QByteArray d)
{
	m_data = d;
	if (m_md5.isEmpty())
	{ m_md5 = QCryptographicHash::hash(m_data, QCryptographicHash::Md5).toHex(); }
}
