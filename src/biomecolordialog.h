#ifndef BIOMECOLORDIALOG_H
#define BIOMECOLORDIALOG_H

#include <QDialog>
#include <QLabel>

namespace Ui {
class BiomeColorDialog;
}
class MainWindow;

class BiomeColorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BiomeColorDialog(MainWindow *parent, QString initrc, int mc, int dim);
    ~BiomeColorDialog();

    int saveColormap(QString rc, QString desc);
    int loadColormap(QString path, bool reset);
    QString getRc();

    void arrange(int sort);

signals:
    void yieldBiomeColorRc(QString path);

public slots:
    void setBiomeColor(int id, const QColor &col);
    void editBiomeColor(int id);

private slots:
    void on_comboColormaps_currentIndexChanged(int index);

    void onSaveAs();
    void onExport();
    void onRemove();
    void onAccept();
    void onImport();
    void onColorHelp();
    void onAllToDefault();
    void onAllToDimmed();


private:
    Ui::BiomeColorDialog *ui;
    MainWindow *mainwindow;
    int mc, dim;
    QLabel *separator;
    QPushButton *buttons[256][3];
    unsigned char colors[256][3];
    QString activerc;
    bool modified;
};

#endif // BIOMECOLORDIALOG_H
