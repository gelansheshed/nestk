#include "image_window.h"
#include "ui_image_window.h"

#include <ntk/utils/time.h>

#include <QMutexLocker>
#include <QHash>

namespace ntk
{

ImagePublisher ImagePublisher::instance;

ImagePublisher *ImagePublisher::getInstance()
{
    // if (!instance)
    //     instance = new ImageWindowManager;
    return &instance;
}

void ImagePublisher::publishImage(const std::string &image_name, const cv::Mat &im)
{
    ImageEventDataPtr data (new ImageEventData);
    data->image_name = image_name;
    im.copyTo(data->im);

    // FIXME: Trick the event system into believing we have one sender per image name.
    EventBroadcaster* fakeSender = reinterpret_cast<EventBroadcaster*>(qHash(image_name.c_str()));

    newEvent(fakeSender, data);
}

QImage
ImagePublisher::getPublishedImage (QString name) const
{
    const QMutexLocker _(const_cast<QMutex*>(&lock));

    images_map_type::const_iterator it = published_images.find(name.toStdString());

    if (it == published_images.end())
    {
        qWarning() << "No such published image: " << name;
        return QImage();
    }

    return it->second->image;
}

void ImagePublisher::handleAsyncEvent(EventListener::Event event)
{
    ImageEventDataPtr internalData = dynamic_Ptr_cast<ImageEventData>(event.data);
    ntk_assert(internalData, "Invalid data type, should not happen");

    PublishedImage* publishedImage = 0;
    {
        const QMutexLocker _(&lock);

        images_map_type::const_iterator it = published_images.find(internalData->image_name);


        if (it == published_images.end())
        {
            publishedImage = new PublishedImage();
            published_images[internalData->image_name] = publishedImage;
            publishedImage->name = QString::fromStdString(internalData->image_name);
        }
        else
        {
            publishedImage = it->second;
        }
    }

    // publishedImage->image = internalData->image;

    switch (internalData->im.type())
    {
    case CV_MAT_TYPE(CV_8UC3): {
        cv::Mat3b mat_ = internalData->im;
        ImageWidget::setImage(publishedImage->image, mat_);
        break;
    }

    case CV_MAT_TYPE(CV_8UC1): {
        cv::Mat1b mat_ = internalData->im;
        ImageWidget::setImage(publishedImage->image, mat_);
        break;
    }

    case CV_MAT_TYPE(CV_32FC1): {
        cv::Mat1f mat_ = internalData->im;
        ImageWidget::setImage(publishedImage->image, mat_);
        break;
    }

        //default:
        //    ntk_dbg(0) << "Unsupported image type";
    }

    PublishedImageEventDataPtr data(new PublishedImageEventData());

    data->image = *publishedImage;

    broadcastEvent(data);
}

//------------------------------------------------------------------------------

ImageWindow::ImageWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::ImageWindow)
{
    ui->setupUi(this);
}

ImageWindow::~ImageWindow()
{
    delete ui;
}

ntk::ImageWidget *ImageWindow::imageWidget()
{
    return ui->centralwidget;
}

void ImageWindow::onImageMouseMoved(int x, int y)
{
    ui->statusbar->showMessage(QString("(%1, %2)").arg(x).arg(y));
}

//------------------------------------------------------------------------------

ImageWindowManager ImageWindowManager::instance;

ImageWindowManager *ImageWindowManager::getInstance()
{
    // if (!instance)
    //     instance = new ImageWindowManager;
    return &instance;
}

void ImageWindowManager::showImage(const std::string &window_name, const cv::Mat &im)
{
    ImageWindowManagerEventDataPtr data (new ImageWindowManagerEventData);
    data->window_name = window_name;
    im.copyTo(data->im);
    newEvent(this, data);
}

void ImageWindowManager::handleAsyncEvent(EventListener::Event event)
{
    ImageWindowManagerEventDataPtr data = dynamic_Ptr_cast<ImageWindowManagerEventData>(event.data);
    ntk_assert(data, "Invalid data type, should not happen");
    windows_map_type::const_iterator it = windows.find(data->window_name);
    ImageWindow* window = 0;
    if (it == windows.end())
    {
        window = new ImageWindow();
        windows[data->window_name] = window;
        window->setWindowTitle(data->window_name.c_str());
        connect(window->imageWidget(), SIGNAL(mouseMoved(int, int)), window, SLOT(onImageMouseMoved(int,int)));
        window->imageWidget()->setRatioKeeping(true);
    }
    else
    {
        window = it->second;
    }

    switch (data->im.type())
    {
    case CV_MAT_TYPE(CV_8UC3): {
        cv::Mat3b mat_ = data->im;
        window->imageWidget()->setImage(mat_);
        break;
    }

    case CV_MAT_TYPE(CV_8UC1): {
        cv::Mat1b mat_ = data->im;
        window->imageWidget()->setImage(mat_);
        break;
    }

    case CV_MAT_TYPE(CV_32FC1): {
        cv::Mat1f mat_ = data->im;
        window->imageWidget()->setImage(mat_);
        break;
    }

    default:
        ntk_dbg(0) << "Unsupported image type";
    }

    window->show();
}

} // ntk
