#include "formconditions.h"
#include "ui_formconditions.h"

#include "conditiondialog.h"
#include "mainwindow.h"

#include <QMessageBox>
#include <QMenu>
#include <QClipboard>


QDataStream& operator<<(QDataStream& out, const Condition& v)
{
    out.writeRawData((const char*)&v, sizeof(Condition));
    return out;
}

QDataStream& operator>>(QDataStream& in, Condition& v)
{
    in.readRawData((char*)&v, sizeof(Condition));
    return in;
}


ItemDelegate::ItemDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

ItemDelegate::~ItemDelegate()
{
}

void ItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    const QWidget *widget = option.widget;
    QStyle *style = widget ? widget->style() : QApplication::style();

    int tabwidth = txtWidth(option.fontMetrics) * 32;
    QRect rect = opt.rect;
    opt.rect.setWidth(tabwidth);

    const QStringList txts = index.data(Qt::DisplayRole).toString().split('\t');
    for (int i = 0, n = txts.size(); i < n; i++)
    {
        if (i == n-1 || opt.rect.right() > rect.right())
            opt.rect.setRight(rect.right());
        opt.text = txts[i];
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);
        opt.rect.adjust(tabwidth, 0, tabwidth, 0);
        if (opt.rect.left() >= rect.right())
            break;
    }
}

FormConditions::FormConditions(QWidget *parent)
    : QWidget(parent)
    , parent(dynamic_cast<MainWindow*>(parent))
    , ui(new Ui::FormConditions)
{
    ui->setupUi(this);

    ui->listConditions->setItemDelegate(new ItemDelegate(this));

    if (!this->parent)
    {
        QLayoutItem *item;
        while ((item = ui->layoutButtons->takeAt(0)))
        {
            delete item->widget();
        }
    }

    qRegisterMetaType< Condition >("Condition");
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    qRegisterMetaTypeStreamOperators< Condition >("Condition");
#endif
}

FormConditions::~FormConditions()
{
    delete ui;
}

QVector<Condition> FormConditions::getConditions() const
{
    QVector<Condition> conds;

    for (int i = 0, ie = ui->listConditions->count(); i < ie; i++)
    {
        Condition c = qvariant_cast<Condition>(ui->listConditions->item(i)->data(Qt::UserRole));
        conds.push_back(c);
    }
    return conds;
}

void FormConditions::updateSensitivity()
{
    if (!parent)
        return;
    QList<QListWidgetItem*> selected = ui->listConditions->selectedItems();

    if (selected.size() == 0)
    {
        ui->buttonRemove->setEnabled(false);
        ui->buttonEdit->setEnabled(false);
    }
    else if (selected.size() == 1)
    {
        ui->buttonRemove->setEnabled(true);
        ui->buttonEdit->setEnabled(true);
    }
    else
    {
        ui->buttonRemove->setEnabled(true);
        ui->buttonEdit->setEnabled(false);
    }

    QVector<Condition> selcond;
    int disabled = 0;
    for (int i = 0; i < selected.size(); i++)
    {
        Condition c = qvariant_cast<Condition>(selected[i]->data(Qt::UserRole));
        selcond.append(c);
        if (c.meta & Condition::DISABLED)
            disabled++;
    }
    if (disabled == 0)
        ui->buttonDisable->setText(tr("Disable"));
    else if (disabled == selected.size())
        ui->buttonDisable->setText(tr("Enable"));
    else
        ui->buttonDisable->setText(tr("Toggle"));
    ui->buttonDisable->setEnabled(!selected.empty());

    emit selectionUpdate(selcond);
}


int FormConditions::getIndex(int idx) const
{
    const QVector<Condition> condvec = getConditions();
    int cnt[100] = {};
    for (const Condition& c : condvec)
        if (c.save >= 0 && c.save < 100)
            cnt[c.save]++;
        else return 0;
    if (idx <= 0)
        idx = 1;
    if (cnt[idx] == 0)
        return idx;
    for (int i = 1; i < 100; i++)
        if (cnt[i] == 0)
            return i;
    return 0;
}

void FormConditions::clearSelection()
{
    ui->listConditions->clearSelection();
}

QListWidgetItem *FormConditions::lockItem(QListWidgetItem *item)
{
    QListWidgetItem *edit = item->clone();
    item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
    item->setSelected(false);
    item->setBackground(QColor(128, 128, 128, 192));
    edit->setData(Qt::UserRole+1, (qulonglong)item);
    return edit;
}

bool list_contains(QListWidget *list, QListWidgetItem *item)
{
    if (!item)
        return false;
    int n = list->count();
    for (int i = 0; i < n; i++)
        if (list->item(i) == item)
            return true;
    return false;
}

void FormConditions::setItemCondition(QListWidget *list, QListWidgetItem *item, Condition *cond)
{
    QListWidgetItem *target = (QListWidgetItem*) item->data(Qt::UserRole+1).toULongLong();
    if (list_contains(list, target) && !(target->flags() & Qt::ItemIsSelectable))
    {
        item->setData(Qt::UserRole+1, (qulonglong)0);
        *target = *item;
        delete item;
        item = target;
    }
    else
    {
        list->addItem(item);
        cond->save = getIndex(cond->save);
    }

    QString s = cond->summary(true);

    const FilterInfo& ft = g_filterinfo.list[cond->type];
    if (ft.cat == CAT_QUAD)
        item->setBackground(QColor(255, 255, 0, 80));

    item->setText(s);
    item->setData(Qt::UserRole, QVariant::fromValue(*cond));
}


void FormConditions::editCondition(QListWidgetItem *item)
{
    if (!(item->flags() & Qt::ItemIsSelectable) || parent == NULL)
        return;
    WorldInfo wi;
    parent->getSeed(&wi);
    ConditionDialog *dialog = new ConditionDialog(this, parent->getMapView(), &parent->config, wi, item, (Condition*)item->data(Qt::UserRole).data());
    QObject::connect(dialog, SIGNAL(setCond(QListWidgetItem*,Condition,int)), this, SLOT(addItemCondition(QListWidgetItem*,Condition,int)), Qt::QueuedConnection);
    dialog->show();
}


static void remove_selected(QListWidget *list)
{
    const QList<QListWidgetItem*> selected = list->selectedItems();
    for (QListWidgetItem *item : selected)
    {
        delete list->takeItem(list->row(item));
    }
}

void FormConditions::on_buttonRemoveAll_clicked()
{
    ui->listConditions->clear();
    emit changed();
}

void FormConditions::on_buttonRemove_clicked()
{
    remove_selected(ui->listConditions);
    emit changed();
}

void FormConditions::on_buttonDisable_clicked()
{
    emit changed();
    const QList<QListWidgetItem*> selected = ui->listConditions->selectedItems();
    for (QListWidgetItem *item : selected)
    {
        Condition c = qvariant_cast<Condition>(item->data(Qt::UserRole));
        c.meta ^= Condition::DISABLED;
        item->setText(c.summary(true));
        item->setData(Qt::UserRole, QVariant::fromValue(c));
    }
    updateSensitivity();
}

void FormConditions::on_buttonEdit_clicked()
{
    QListWidgetItem* edit = 0;
    QList<QListWidgetItem*> selected = ui->listConditions->selectedItems();

    if (!selected.empty())
        edit = lockItem(selected.first());

    if (edit)
        editCondition(edit);
}

void FormConditions::on_buttonAddFilter_clicked()
{
    if (!parent)
        return;
    WorldInfo wi;
    parent->getSeed(&wi);
    ConditionDialog *dialog = new ConditionDialog(this, parent->getMapView(), &parent->config, wi);
    QObject::connect(dialog, SIGNAL(setCond(QListWidgetItem*,Condition,int)), this, SLOT(addItemCondition(QListWidgetItem*,Condition)), Qt::QueuedConnection);
    dialog->show();
}

void FormConditions::on_listConditions_customContextMenuRequested(const QPoint &pos)
{
    QMenu menu(this);
    // this is a contextual temporary menu so shortcuts are only indicated here,
    // but will not function - see keyReleaseEvent() for shortcut implementation

    int n = ui->listConditions->selectedItems().size();

    if (parent)
    {
       QAction *actadd = menu.addAction(QIcon::fromTheme("list-add"),
            tr("Add new condition"), this,
            &FormConditions::conditionsAdd, QKeySequence::New);
        actadd->setEnabled(true);

        QAction *actedit = menu.addAction(QIcon(),
            tr("Edit condition"), this,
            &FormConditions::conditionsEdit, QKeySequence::Open);
        actedit->setEnabled(n == 1);

        QAction *actcut = menu.addAction(QIcon::fromTheme("edit-cut"),
            tr("Cut %n condition(s)", "", n), this,
            &FormConditions::conditionsCut, QKeySequence::Cut);
        actcut->setEnabled(n > 0);

        QAction *actcopy = menu.addAction(QIcon::fromTheme("edit-copy"),
            tr("Copy %n condition(s)", "", n), this,
            &FormConditions::conditionsCopy, QKeySequence::Copy);
        actcopy->setEnabled(n > 0);

        int pn = conditionsPaste(true);
        QAction *actpaste = menu.addAction(QIcon::fromTheme("edit-paste"),
            tr("Paste %n condition(s)", "", pn), this,
            &FormConditions::conditionsPaste, QKeySequence::Paste);
        actpaste->setEnabled(pn > 0);

        QAction *actdel = menu.addAction(QIcon::fromTheme("edit-delete"),
            tr("Remove %n condition(s)", "", n), this,
            &FormConditions::conditionsDelete, QKeySequence::Delete);
        actdel->setEnabled(n > 0);
    }
    else
    {
        QAction *actcopy = menu.addAction(QIcon::fromTheme("edit-copy"),
            tr("Copy %n condition(s)", "", n), this,
            &FormConditions::conditionsCopy, QKeySequence::Copy);
        actcopy->setEnabled(n > 0);
    }

    menu.exec(ui->listConditions->mapToGlobal(pos));
}

void FormConditions::on_listConditions_itemDoubleClicked(QListWidgetItem *item)
{
    if (!parent)
        return;
    editCondition(lockItem(item));
}

void FormConditions::on_listConditions_itemSelectionChanged()
{
    updateSensitivity();
}

void FormConditions::conditionsAdd()
{
    if (!parent)
        return;
    on_buttonAddFilter_clicked();
}

void FormConditions::conditionsEdit()
{
    if (!parent)
        return;
    on_buttonEdit_clicked();
}

void FormConditions::conditionsCut()
{
    if (!parent)
        return;
    conditionsCopy();
    on_buttonRemove_clicked();
}

void FormConditions::conditionsCopy()
{
    QString text;
    const QList<QListWidgetItem*> selected = ui->listConditions->selectedItems();
    for (QListWidgetItem *item : selected)
    {
        Condition c = qvariant_cast<Condition>(item->data(Qt::UserRole));
        text += (text.isEmpty() ? "" : "\n") + c.toHex();
    }
    QClipboard *clipboard = QGuiApplication::clipboard();
    clipboard->setText(text);
}

int FormConditions::conditionsPaste(bool countonly)
{
    if (!parent)
        return 0;
    QClipboard *clipboard = QGuiApplication::clipboard();
    const QStringList slist = clipboard->text().split('\n');
    Condition c;
    int cnt = 0;
    for (const QString& s : slist)
    {
        if (!c.readHex(s))
            continue;
        cnt++;
        if (countonly)
            continue;
        addItemCondition(NULL, c, 1);
    }
    return cnt;
}

void FormConditions::conditionsDelete()
{
    if (!parent)
        return;
    on_buttonRemove_clicked();
}

void FormConditions::addItemCondition(QListWidgetItem *item, Condition cond, int modified)
{
    const FilterInfo& ft = g_filterinfo.list[cond.type];

    if (ft.cat != CAT_QUAD)
    {
        if (!item) {
            item = new QListWidgetItem();
            modified = 1;
        }
        setItemCondition(ui->listConditions, item, &cond);
    }
    else if (item)
    {
        setItemCondition(ui->listConditions, item, &cond);
    }
    else
    {
        modified = 1;
        item = new QListWidgetItem();
        setItemCondition(ui->listConditions, item, &cond);

        if (cond.type >= F_QH_IDEAL && cond.type <= F_QH_BARELY)
        {
            Condition cq;
            memset(&cq, 0, sizeof(cq));
            cq.type = F_HUT;
            //cq.x1 = -128; cq.z1 = -128;
            //cq.x2 = +128; cq.z2 = +128;
            // use 256 to avoid confusion when this restriction is removed
            cq.rmax = 256;
            cq.relative = cond.save;
            cq.save = cond.save+1;
            cq.count = 4;
            QListWidgetItem *item = new QListWidgetItem(ui->listConditions, QListWidgetItem::UserType);
            setItemCondition(ui->listConditions, item, &cq);
        }
        else if (cond.type == F_QM_90 || cond.type == F_QM_95)
        {
            Condition cq;
            memset(&cq, 0, sizeof(cq));
            cq.type = F_MONUMENT;
            //cq.x1 = -160; cq.z1 = -160;
            //cq.x2 = +160; cq.z2 = +160;
            // use 256 to avoid confusion when this restriction is removed
            cq.rmax = 256;
            cq.relative = cond.save;
            cq.save = cond.save+1;
            cq.count = 4;
            QListWidgetItem *item = new QListWidgetItem(ui->listConditions, QListWidgetItem::UserType);
            setItemCondition(ui->listConditions, item, &cq);
        }
    }

    if (modified)
        emit changed();
    updateSensitivity();
}

void FormConditions::on_listConditions_indexesMoved(const QModelIndexList &)
{
    emit changed();
}

void FormConditions::keyReleaseEvent(QKeyEvent *event)
{
    if (ui->listConditions->hasFocus())
    {
        if (event->matches(QKeySequence::New))
            conditionsAdd();
        else if (event->matches(QKeySequence::Open))
            conditionsEdit();
        else if (event->matches(QKeySequence::Cut))
            conditionsCut();
        else if (event->matches(QKeySequence::Copy))
            conditionsCopy();
        else if (event->matches(QKeySequence::Paste))
            conditionsPaste();
        else if (event->matches(QKeySequence::Delete))
            conditionsDelete();
    }
    QWidget::keyReleaseEvent(event);
}


