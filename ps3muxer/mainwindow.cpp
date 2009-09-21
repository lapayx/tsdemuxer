#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "ebml.h"
#include <QFileDialog>
#include <QMessageBox>
#include <stdio.h>
#include <QStyleFactory>
#include "version.h"

#ifdef _WIN32
#define os_slash    '\\'
#else
#define os_slash    '/'
#endif

#ifdef _WIN32
static const char conf_path[]="ps3muxer_win32.cfg";
#else
static const char conf_path[]="ps3muxer.cfg";
#endif

MainWindow::MainWindow(QWidget *parent,const QString& cmd)
    : QMainWindow(parent), ui(new Ui::MainWindowClass)
{
    ui->setupUi(this);
    ebml::init();

    batch_size=0;

    this->setWindowTitle(QString("%1 - %2").arg(MyAppVerName).arg(MyAppPublisher));

    ui->tableWidget->horizontalHeader()->setStretchLastSection(true);
    ui->tableWidget_2->horizontalHeader()->setStretchLastSection(true);
    ui->tableWidget->setSelectionBehavior ( QAbstractItemView::SelectRows );
    ui->tableWidget_2->setSelectionBehavior ( QAbstractItemView::SelectRows );

    connect(ui->actionOpen,SIGNAL(triggered()),this,SLOT(on_pushButton_clicked()));
    connect(ui->actionClear,SIGNAL(triggered()),this,SLOT(on_pushButton_3_clicked()));
    connect(ui->actionStart_muxing,SIGNAL(triggered()),this,SLOT(on_pushButton_2_clicked()));
    connect(ui->actionQuit,SIGNAL(triggered()),this,SLOT(close()));
    connect(ui->actionAbout,SIGNAL(triggered()),this,SLOT(aboutBox()));
    connect(ui->actionAdd_to_batch,SIGNAL(triggered()),this,SLOT(addToBatch()));
    connect(ui->actionClear_batch,SIGNAL(triggered()),this,SLOT(clearBatch()));

    FILE* fp=fopen((QApplication::applicationDirPath()+os_slash+conf_path).toLocal8Bit().data(),"r");
    if(fp)
    {
        char tmp[1024];
        while(fgets(tmp,sizeof(tmp),fp))
        {
            char* p=tmp;

            {
                char* zz=strpbrk(p,"#;\r\n");
                if(zz)
                    *zz=0;
            }

            while(*p && (*p==' ' || *p=='\t'))
                p++;

            if(!*p)
                continue;

            char* p2=strchr(p,'=');
            if(!p2)
                continue;

            char* pp=p2;

            *p2=0;
            p2++;

            while(pp>p && (pp[-1]==' ' || pp[-1]=='\t'))
                pp--;
            *pp=0;

            while(*p2 && (*p2==' ' || *p2=='\t'))
                p2++;

            pp=p2+strlen(p2);

            while(pp>p2 && (pp[-1]==' ' || pp[-1]=='\t'))
                pp--;
            *pp=0;

            if(*p && *p2)
            {
                if(!strncmp(p,"split_",6))
                    ui->comboBox->addItem(QString(p+6).replace('_',' '),QVariant(p2));
                else if(!strncmp(p,"codec_",6))
                    initCodec(p2,p+6);
                else
                    cfg[p]=p2;
            }
        }
        fclose(fp);
    }

    QStringList nc=QString(cfg["native_codecs"].c_str()).split(',',QString::SkipEmptyParts);
    for(int i=0;i<nc.size();i++)
        native_codecs[nc[i].toLocal8Bit().data()]=1;


    const std::string& style=cfg["style"];
    if(style.length())
        QApplication::setStyle(QStyleFactory::create(style.c_str()));

    src_file_name=cmd;

    if(!src_file_name.isEmpty())
        on_pushButton_clicked();

}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::initCodec(const std::string& s,const std::string pn)
{
    std::vector<std::string> ss;
    ss.reserve(3);

    int n=0;
    for(std::string::size_type p1=0,p2;p1!=std::string::npos && n<3;p1=p2,n++)
    {
        std::string c;

        p2=s.find_first_of(':',p1);
        if(p2!=std::string::npos)
        {
            c=s.substr(p1,p2-p1);
            p2++;
        }else
            c=s.substr(p1);

        ss.push_back(c);
    }

    codec cc;

    cc.name=ss[0];
    cc.map=ss[1].length()?ss[1]:cc.name;
    cc.file_ext=ss[2];
    cc.print_name=pn;

    if(!strncmp(cc.name.c_str(),"V_",2))
        cc.type=1;
    else if(!strncmp(cc.name.c_str(),"A_",2))
        cc.type=2;

    if(cc.name.length() && cc.type)
        codecs[cc.name]=cc;
}

void MainWindow::addRow(QTableWidget* w,const QStringList& l)
{
    int column=0;
    int row=w->rowCount();
    w->setRowCount(row+1);
    Q_FOREACH(QString s,l)
    {

        QTableWidgetItem* newItem=new QTableWidgetItem(s);
        w->setItem(row,column++,newItem);
    }
}

void MainWindow::on_pushButton_clicked()
{
    QString path;

    if(src_file_name.size())
    {
        path=src_file_name;
        src_file_name.clear();
    }
    else
        path=QFileDialog::getOpenFileName(this,tr("MKV source file"),QString::fromLocal8Bit(last_dir.c_str()),"MKV file (*.mkv)");

#ifdef _WIN32
    path=path.replace('/','\\');
#endif

    if(!path.isEmpty())
    {
        ui->tableWidget->setRowCount(0);
        ui->tableWidget_2->setRowCount(0);

        try
        {
            ebml::stream stream(path.toLocal8Bit().data());

            ebml::doc m;

            ebml::parse(&stream,m);

            if(strcasecmp(m.doc_type.c_str(),"matroska"))
                throw(ebml::exception("not a matroska"));

//            if(m.version!=1 && m.read_version!=1)
//                throw(ebml::exception("unknown matroska version"));

            for(std::map<u_int32_t,ebml::track>::const_iterator i=m.tracks.begin();i!=m.tracks.end();++i)
            {
                const ebml::track& t=i->second;

                QStringList lst;
                lst<<QString("%1").arg(t.id)<<QString("%1").arg(t.timecode==-1?0:t.timecode)<<t.lang.c_str()<<t.codec.c_str();

                const codec& cc=codecs[t.codec];

                if(cc.type==1)
                        addRow(ui->tableWidget,lst);
                else if(cc.type==2)
                        addRow(ui->tableWidget_2,lst);
            }

            if(ui->tableWidget->rowCount()>0)
                ui->tableWidget->setCurrentCell(0,0);

            if(ui->tableWidget_2->rowCount()>0)
                ui->tableWidget_2->setCurrentCell(0,0);

            ui->tableWidget->resizeColumnsToContents();
            ui->tableWidget_2->resizeColumnsToContents();
            ui->tableWidget->horizontalHeader()->setStretchLastSection(true);
            ui->tableWidget_2->horizontalHeader()->setStretchLastSection(true);

            {
                std::string s1=path.toLocal8Bit().data();
                std::string s2;

                std::string::size_type n=s1.find_last_of("\\/");
                if(n!=std::string::npos)
                {
                    s2=s1.substr(n+1);
                    s1=s1.substr(0,n);
                }
                else
                {
                    s2=s1;
                    s1="";
                }

                n=s2.find_last_of('.');
                if(n==std::string::npos)
                    s2=s2+".m2ts";
                else
                    s2=s2.substr(0,n)+".m2ts";

                if(s1.length())
                    s1+=os_slash;

                last_dir=s1;
                if(!last_dir_dst.length())
                    last_dir_dst=s1;

                ui->lineEdit->setText(QString::fromLocal8Bit((last_dir_dst+s2).c_str()));
            }

            source_file_name=path.toLocal8Bit().data();
        }
        catch(const std::exception& e)
        {
            QMessageBox::warning(this,tr("Error"),tr("Open file error: %1").arg(e.what()));
        }
    }
}

void MainWindow::on_pushButton_3_clicked()
{
    ui->tableWidget->setRowCount(0);
    ui->tableWidget_2->setRowCount(0);
    ui->lineEdit->clear();
    source_file_name.clear();
    ui->label_5->clear();
    ui->label_6->clear();
    //ui->comboBox->setCurrentIndex(0);
}

void MainWindow::on_pushButton_4_clicked()
{
    QString cur_path=ui->lineEdit->text();
    if(!cur_path.size())
        cur_path=QString::fromLocal8Bit(last_dir_dst.c_str());

    QString path=QFileDialog::getSaveFileName(this,tr("M2TS target file"),cur_path,"M2TS file (*.m2ts)");

#ifdef _WIN32
    path=path.replace('/','\\');
#endif

    if(!path.isEmpty())
    {
        std::string s=path.toLocal8Bit().data();
        std::string s1;

        std::string::size_type n=s.find_last_of("\\/");
        if(n!=std::string::npos)
        {
            s1=s.substr(0,n);
            s=s.substr(n+1);
        }

        if(s1.length())
            s1+=os_slash;

        last_dir_dst=s1;

        n=s.find_last_of('.');
        if(n==std::string::npos)
            path=path+".m2ts";

        ui->lineEdit->setText(path);
    }
}

void MainWindow::parseCmdParams(const QString& s,QStringList& lst)
{
    lst<<s.split(' ',QString::SkipEmptyParts);
}

void MainWindow::on_pushButton_2_clicked()
{
    if(!global_batch.size())
        startMuxing(false);
    else
    {
        addToBatch();
        QMessageBox mbox(QMessageBox::Question,tr("Batch processing"),tr("%1 tasks will be processed, you are assured?").arg(batch_size),QMessageBox::Yes|QMessageBox::No,this);
        if(mbox.exec()==QMessageBox::No)
            return;

        execWindow dlg(this);
        dlg.batch.swap(global_batch);
        dlg.run();
        global_batch.clear();
        batch_size=0;
    }
}

void MainWindow::addToBatch()
{
    startMuxing(true);
    on_pushButton_3_clicked();
}
void MainWindow::clearBatch()
{
    if(global_batch.size())
    {
        global_batch.clear();
        QMessageBox::warning(this,tr("Done"),tr("The batch is empty"));
    }
    batch_size=0;
}

void MainWindow::startMuxing(bool delay)
{
    std::list<execCmd>      batch;
    std::list<std::string>  tmp_files;

    int _video=ui->tableWidget->currentRow();

    if(_video==-1)
        return;

    // get tmp path
    std::string tmp_path=cfg["tmp_path"];
    if(!tmp_path.length())
        tmp_path=(QDir::tempPath()+os_slash).toLocal8Bit().data();

    // split value
    std::string split=ui->comboBox->itemData(ui->comboBox->currentIndex()).toString().toLocal8Bit().data();

    // remove tmp files
    bool remove_tmp_files=true;

    {
        const std::string& s=cfg["remove_tmp_files"];
        if(s=="false" || s=="FALSE" || s=="0")
            remove_tmp_files=false;
        else if(s=="true" || s=="TRUE" || s=="1")
            remove_tmp_files=true;
    }

    std::string prefix;

    {
        std::string::size_type n=source_file_name.find_last_of("\\/");
        if(n!=std::string::npos)
            prefix=source_file_name.substr(n+1);
        else
            prefix=source_file_name;

        n=prefix.find_last_of('.');
        if(n!=std::string::npos)
            prefix=prefix.substr(0,n);
    }

    track_info              video_track;
    std::list<track_info>   audio_tracks;

    // get video track info
    video_track.track_id=ui->tableWidget->item(_video,0)->text().toLocal8Bit().data();
    video_track.delay=atoi(ui->tableWidget->item(_video,1)->text().toLocal8Bit().data());
    video_track.lang=ui->tableWidget->item(_video,2)->text().toLocal8Bit().data();
    video_track.codec=ui->tableWidget->item(_video,3)->text().toLocal8Bit().data();

    // get audio tracks info
    {
        std::map<int,int> _audio_map;

        Q_FOREACH(QTableWidgetItem* _audio_item,ui->tableWidget_2->selectedItems())
        {
            int row=_audio_item->row();

            int& n=_audio_map[row];

            if(!n)
            {
                audio_tracks.push_back(track_info());
                track_info& t=audio_tracks.back();

                t.track_id=ui->tableWidget_2->item(row,0)->text().toLocal8Bit().data();
                t.delay=atoi(ui->tableWidget_2->item(row,1)->text().toLocal8Bit().data());
                t.lang=ui->tableWidget_2->item(row,2)->text().toLocal8Bit().data();
                t.codec=ui->tableWidget_2->item(row,3)->text().toLocal8Bit().data();

                t.delay=t.delay-video_track.delay;

                n=1;
            }
        }
    }

    // unknown video track
    if(native_codecs[codecs[video_track.codec].print_name]!=1)
    {
        QMessageBox mbox(QMessageBox::Question,tr("Unknown video"),tr("You have chosen %1 video track, PS3 will not show it, you are assured?").arg(video_track.codec.c_str()),QMessageBox::Yes|QMessageBox::No,this);
        if(mbox.exec()==QMessageBox::No)
            return;
    }

    // prepare audio

    int transcode_audio_track_num=0;

    for(std::list<track_info>::iterator i=audio_tracks.begin();i!=audio_tracks.end();++i)
    {
        track_info& audio_track=*i;

        std::string audio_file_name;
        std::string audio_file_name2;

        const codec& cc=codecs[audio_track.codec];

        if(native_codecs[cc.print_name]!=1)
        {
            // transcode to AC3

            QMessageBox mbox(QMessageBox::Question,tr("Transcoding audio"),tr("You have chosen %1 audio track, it will be converted, process will occupy approximately 15 minutes, you are assured?").arg(audio_track.codec.c_str()),QMessageBox::Yes|QMessageBox::No,this);
            if(mbox.exec()==QMessageBox::No)
                return;

            audio_file_name=tmp_path+prefix+"_track_"+audio_track.track_id+'.';
            audio_file_name2=tmp_path+prefix+"_track_"+audio_track.track_id+"_encoded.ac3";

            audio_file_name+=cc.file_ext.length()?cc.file_ext:"bin";

            tmp_files.push_back(audio_file_name);
            tmp_files.push_back(audio_file_name2);

            audio_track.filename=audio_file_name2;
            audio_track.filename_temp=audio_file_name;

            transcode_audio_track_num++;
        }
    }

    if(transcode_audio_track_num)
    {
        QStringList lst;

        lst<<"tracks"<<QString::fromLocal8Bit((source_file_name.c_str()));

        for(std::list<track_info>::iterator i=audio_tracks.begin();i!=audio_tracks.end();++i)
        {
            track_info& audio_track=*i;

            if(audio_track.filename.length())
                lst<<QString("%1:%2").arg(audio_track.track_id.c_str()).arg(QString::fromLocal8Bit(audio_track.filename_temp.c_str()));
        }

        batch.push_back(execCmd(tr("Extracting audio tracks"),
            tr("Extracting audio tracks (approximately 3-5 min for 8Gb movie)..."),
            cfg["mkvextract"].c_str(),lst));


        for(std::list<track_info>::iterator i=audio_tracks.begin();i!=audio_tracks.end();++i)
        {
            track_info& audio_track=*i;

            if(audio_track.filename.length())
            {
                lst.clear();

                const std::string& encoder_args=cfg["encoder_args"];

                if(encoder_args.length())
                    parseCmdParams(encoder_args.c_str(),lst);

                for(int i=0;i<lst.size();i++)
                {
                    QString& s=lst[i];
                    if(s=="%1")
                        s=QString::fromLocal8Bit(audio_track.filename_temp.c_str());
                    else if(s=="%2")
                        s=QString::fromLocal8Bit(audio_track.filename.c_str());
                }

                batch.push_back(execCmd(tr("Encoding audio track"),
                    tr("Encoding audio track %1 to AC3 (approximately 10-20 min for 8Gb movie)...").arg(audio_track.track_id.c_str()),
                    cfg["encoder"].c_str(),lst));
            }
        }
    }

    std::string meta=tmp_path+prefix+".meta";

    FILE* fp=fopen(meta.c_str(),"w");
    if(fp)
    {
        tmp_files.push_back(meta);

        std::string opts="MUXOPT --no-pcr-on-video-pid --new-audio-pes --vbr --vbv-len=500";
        if(split.length())
            opts+=" --split-size="+split;
        opts+="\n";

        const std::string& vcc=codecs[video_track.codec].map;
        const std::string& level=cfg["h264_level"];

        opts+=(vcc.length()?vcc:video_track.codec)+", \""+source_file_name+"\", insertSEI, contSPS, track="+video_track.track_id;
        if(video_track.lang.length())
            opts+=", lang="+video_track.lang;
        if(level.length())
            opts+=", level="+level;
        opts+="\n";


        for(std::list<track_info>::iterator i=audio_tracks.begin();i!=audio_tracks.end();++i)
        {
            track_info& audio_track=*i;

            const std::string& acc=codecs[audio_track.codec].map;

            if(audio_track.filename.length())
                opts+="A_AC3, \""+audio_track.filename+"\"";
            else
                opts+=(acc.length()?acc:audio_track.codec)+", \""+source_file_name+"\", track="+audio_track.track_id;
            if(audio_track.lang.length())
                opts+=", lang="+audio_track.lang;
            if(audio_track.delay)
            {
                char t[32]; sprintf(t,", timeshift=%ims",audio_track.delay);
                opts+=t;
            }
            opts+="\n";
        }

        fprintf(fp,"%s",opts.c_str());

        fclose(fp);


        QStringList lst;

        lst<<QString::fromLocal8Bit(meta.c_str())<<ui->lineEdit->text();


        batch.push_back(execCmd(tr("Remux M2TS"),
            tr("Muxing to MPEG2-TS stream (approximately 5 min for 8Gb movie)...\n%1").arg(QString::fromLocal8Bit(opts.c_str())),
               cfg["tsmuxer"].c_str(),lst));

        if(!delay)
        {
            execWindow dlg(this);
            dlg.batch.swap(batch);
            dlg.run();

            if(remove_tmp_files)
                for(std::list<std::string>::iterator i=tmp_files.begin();i!=tmp_files.end();++i)
                    remove(i->c_str());
        }else
        {
            if(remove_tmp_files)
                for(std::list<std::string>::iterator i=tmp_files.begin();i!=tmp_files.end();++i)
                {
                    const std::string& s=cfg["rm"];
                    if(s.length())
                    {
                        QStringList lst;
                        QString filename=QString::fromLocal8Bit(i->c_str());

#ifdef _WIN32
                        filename=filename.replace('/','\\');
#endif

                        lst<<filename;
                        batch.push_back(execCmd(tr("Remove file"),
                            tr("Remove temp file '%1'...").arg(lst[0]),s.c_str(),lst));
                    }
                }

            for(std::list<execCmd>::iterator i=batch.begin();i!=batch.end();++i)
                global_batch.push_back(*i);

            QMessageBox::warning(this,tr("Done"),tr("The task number %1 is successfully added").arg(++batch_size));
        }
    }else
        QMessageBox::warning(this,tr("Error"),tr("Unable to create meta file: %1").arg(meta.c_str()));
}


void MainWindow::on_tableWidget_itemSelectionChanged()
{
    int row=ui->tableWidget->currentRow();
    if(row==-1)
        return;

    ui->label_5->setText(QString("[%1]").arg(codecs[ui->tableWidget->item(row,3)->text().toLocal8Bit().data()].print_name.c_str()));
}


void MainWindow::on_tableWidget_2_itemSelectionChanged()
{
    int row=ui->tableWidget_2->currentRow();
    if(row==-1)
        return;

    ui->label_6->setText(QString("[%1]").arg(codecs[ui->tableWidget_2->item(row,3)->text().toLocal8Bit().data()].print_name.c_str()));
}
void MainWindow::aboutBox()
{
    QMessageBox::about(this,tr("About"),QString("<b>%1</b><br><br>Copyright (C) 2009 %2. All rights reserved.<br><font size=-1><br>E-Mail: <a href='mailto:clark15b@gmail.com'>clark15b@gmail.com</a><br>Web: <a href='http://ps3muxer.org'>%3</a></font>").arg(MyAppVerName).arg(MyAppPublisher).arg(MyAppURL));
}
