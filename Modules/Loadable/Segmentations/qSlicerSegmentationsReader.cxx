/*==============================================================================

  Copyright (c) Laboratory for Percutaneous Surgery (PerkLab)
  Queen's University, Kingston, ON, Canada. All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Adam Rankin, PerkLab, Queen's University
  and was supported through the Applied Cancer Research Unit program of Cancer Care
  Ontario with funds provided by the Ontario Ministry of Health and Long-Term Care

==============================================================================*/

// Segmentations includes
#include "qSlicerSegmentationsReader.h"
#include "qSlicerSegmentationsIOOptionsWidget.h"

// Qt includes
#include <QFileInfo>

// Logic includes
#include "vtkSlicerSegmentationsModuleLogic.h"

// MRML includes
#include <vtkMRMLScene.h>
#include <vtkMRMLSegmentationNode.h>
#include <vtkMRMLSegmentationDisplayNode.h>
#include <vtkMRMLStorageNode.h>

// VTK includes
#include <vtkNew.h>
#include <vtkSmartPointer.h>
#include <vtkSTLReader.h>

//-----------------------------------------------------------------------------
class qSlicerSegmentationsReaderPrivate
{
public:
  vtkSmartPointer<vtkSlicerSegmentationsModuleLogic> SegmentationsLogic;
};

//-----------------------------------------------------------------------------
/// \ingroup Slicer_QtModules_Segmentations
qSlicerSegmentationsReader::qSlicerSegmentationsReader(vtkSlicerSegmentationsModuleLogic* segmentationsLogic, QObject* _parent)
  : Superclass(_parent)
  , d_ptr(new qSlicerSegmentationsReaderPrivate)
{
  this->setSegmentationsLogic(segmentationsLogic);
}

//-----------------------------------------------------------------------------
qSlicerSegmentationsReader::~qSlicerSegmentationsReader()
{
}

//-----------------------------------------------------------------------------
void qSlicerSegmentationsReader::setSegmentationsLogic(vtkSlicerSegmentationsModuleLogic* newSegmentationsLogic)
{
  Q_D(qSlicerSegmentationsReader);
  d->SegmentationsLogic = newSegmentationsLogic;
}

//-----------------------------------------------------------------------------
vtkSlicerSegmentationsModuleLogic* qSlicerSegmentationsReader::segmentationsLogic()const
{
  Q_D(const qSlicerSegmentationsReader);
  return d->SegmentationsLogic;
}

//-----------------------------------------------------------------------------
QString qSlicerSegmentationsReader::description()const
{
  return "Segmentation";
}

//-----------------------------------------------------------------------------
qSlicerIO::IOFileType qSlicerSegmentationsReader::fileType()const
{
  return QString("SegmentationFile");
}

//-----------------------------------------------------------------------------
QStringList qSlicerSegmentationsReader::extensions()const
{
  return QStringList() << "Segmentation (*.seg.nrrd)" << "Segmentation (*.seg.vtm)"
    << "Segmentation (*.nrrd)" << "Segmentation (*.vtm)" << "Segmentation (*.stl)";
}

//-----------------------------------------------------------------------------
qSlicerIOOptions* qSlicerSegmentationsReader::options()const
{
  qSlicerIOOptionsWidget* options = new qSlicerSegmentationsIOOptionsWidget;
  options->setMRMLScene(this->mrmlScene());
  return options;
}

//-----------------------------------------------------------------------------
bool qSlicerSegmentationsReader::load(const IOProperties& properties)
{
  Q_D(qSlicerSegmentationsReader);
  Q_ASSERT(properties.contains("fileName"));
  QString fileName = properties["fileName"].toString();

  this->setLoadedNodes(QStringList());
  if (d->SegmentationsLogic.GetPointer() == 0)
    {
    return false;
    }

  std::string extension = vtkMRMLStorageNode::GetLowercaseExtensionFromFileName(fileName.toStdString());

  if (extension.compare(".stl") == 0)
    {
    // Create segmentation from STL file
    vtkNew<vtkSTLReader> reader;
    reader->SetFileName(fileName.toStdString().c_str());
    reader->Update();
    vtkPolyData* closedSurfaceRepresentation = reader->GetOutput();
    if (!closedSurfaceRepresentation)
      {
      return false;
      }

    QString name = QFileInfo(fileName).baseName();
    if (properties.contains("name"))
      {
      name = properties["name"].toString();
      }

    vtkNew<vtkSegment> segment;
    segment->SetName(name.toLatin1().constData());
    segment->AddRepresentation(vtkSegmentationConverter::GetSegmentationClosedSurfaceRepresentationName(), closedSurfaceRepresentation);

    vtkMRMLSegmentationNode* segmentationNode = vtkMRMLSegmentationNode::SafeDownCast(
      this->mrmlScene()->AddNewNodeByClass("vtkMRMLSegmentationNode", name.toLatin1().constData()));
    segmentationNode->SetMasterRepresentationToClosedSurface();
    segmentationNode->CreateDefaultDisplayNodes();
    vtkMRMLSegmentationDisplayNode* displayNode = vtkMRMLSegmentationDisplayNode::SafeDownCast(segmentationNode->GetDisplayNode());
    if (displayNode)
      {
      // Show slice intersections using closed surface representation (don't create binary labelmap for display)
      displayNode->SetPreferredDisplayRepresentationName2D(vtkSegmentationConverter::GetSegmentationClosedSurfaceRepresentationName());
      }

    segmentationNode->GetSegmentation()->AddSegment(segment.GetPointer());

    this->setLoadedNodes(QStringList(QString(segmentationNode->GetID())));
    }
  else
    {
    // Non-STL file, load using segmentation storage node
    bool autoOpacities = true;
    if (properties.contains("autoOpacities"))
      {
      autoOpacities = properties["autoOpacities"].toBool();
      }

    vtkMRMLSegmentationNode* node = d->SegmentationsLogic->LoadSegmentationFromFile(fileName.toLatin1().constData(), autoOpacities);
    if (!node)
      {
      this->setLoadedNodes(QStringList());
      return false;
      }

    this->setLoadedNodes( QStringList(QString(node->GetID())) );

    if (properties.contains("name"))
      {
      std::string uname = this->mrmlScene()->GetUniqueNameByString(properties["name"].toString().toLatin1());
      node->SetName(uname.c_str());
      }
    }

  return true;
}
