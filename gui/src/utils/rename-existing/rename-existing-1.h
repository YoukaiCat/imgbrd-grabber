#ifndef RENAME_EXISTING_1_H
#define RENAME_EXISTING_1_H

#include <QDialog>
#include <QMap>
#include "models/site.h"
#include "models/filename.h"
#include "image-renaming-data.h"



namespace Ui
{
	class RenameExisting1;
}

class RenameExisting1 : public QDialog
{
	Q_OBJECT

	public:
		explicit RenameExisting1(Profile *profile, QMap<QString,Site*> sites, QWidget *parent = Q_NULLPTR);
		~RenameExisting1();

	private slots:
		void getAll(Page *p);
		void getTags();
		void loadNext();
		void on_buttonCancel_clicked();
		void on_buttonContinue_clicked();

	private:
		Ui::RenameExisting1						*ui;
		Profile									*m_profile;
		QMap<QString, Site*>					m_sites;
		Filename								m_filename;
		bool									m_needDetails;
		QList<QMap<QString, QString>>			m_details;
		QList<QSharedPointer<Image>>			m_getTags;
		QMap<QString, ImageRenamingData>		m_getAll;
};

#endif // RENAME_EXISTING_1_H
