#pragma once

#include <nanogui/screen.h>
#include <memory>

#include "BSDFCanvas.h"
#include "DataSampleButton.h"
#include "MetadataWindow.h"
#include "HelpWindow.h"
#include "ColorMap.h"

class BSDFApplication : public nanogui::Screen {
public:
    BSDFApplication();
    ~BSDFApplication();

    virtual bool keyboardEvent(int key, int scancode, int action, int modifiers) override;

    void drawContents() override;
    void draw(NVGcontext *ctx) override;
    void requestLayoutUpdate() { m_RequiresLayoutUpdate = true; }

    void openDataSampleDialog();
    void saveScreenShot();
    void toggleMetadataWindow();
    void toggleHelpWindow();
    void deleteDataSample(std::shared_ptr<DataSample> dataSample);
    void selectDataSample(std::shared_ptr<DataSample> dataSample);
    void selectDataSample(int index, bool clamped = true);

    int dataSampleIndex(const std::shared_ptr<const DataSample> dataSample) const;
    
    bool hasSelectedDataSample() const                  { return m_SelectedDataSample != nullptr; }
    std::shared_ptr<DataSample> selectedDataSample()    { return m_SelectedDataSample; }
    const std::shared_ptr<const DataSample> selectedDataSample() const { return m_SelectedDataSample; }
    int selectedDataSampleIndex() const { return dataSampleIndex(m_SelectedDataSample); }

    DataSampleButton* correspondingButton(const std::shared_ptr<const DataSample> dataSample);
    const DataSampleButton* correspondingButton(const std::shared_ptr<const DataSample> dataSample) const;

private:
    void BSDFApplication::toggleView(DataSample::Views view, std::shared_ptr<DataSample> dataSample);

    void updateLayout();
    void addDataSampleButton(int index, std::shared_ptr<DataSample> dataSample);

    void toggleToolButton(nanogui::Button* button, bool needsSelectedDataSample);


    bool m_RequiresLayoutUpdate = false;

    nanogui::Window* m_ToolWindow;
    nanogui::Widget* m_3DView;
    
    // footer
    nanogui::Widget* m_Footer;
    nanogui::Label* m_DataSampleName;
    nanogui::Label* m_DataSamplePointsCount;
    nanogui::Label* m_DataSampleAverageHeight;

    // data sample scroll panel
    nanogui::VScrollPanel* m_DataSamplesScrollPanel;
    nanogui::Widget* m_ScrollContent;
    nanogui::Widget* m_DataSampleButtonContainer;

    // tool buttons
    nanogui::Button* m_HelpButton;
    nanogui::Button* m_GridViewToggle;
    nanogui::Button* m_OrthoViewToggle;

    // dialog windows
    nanogui::Window* m_MetadataWindow;
    HelpWindow* m_HelpWindow;

    // canvas
    BSDFCanvas *m_BSDFCanvas;

    std::vector<std::shared_ptr<DataSample>> m_DataSamples;
    std::shared_ptr<DataSample> m_SelectedDataSample;
    std::vector<std::shared_ptr<ColorMap>> m_ColorMaps;

    // offscreen buffer
    nanogui::GLFramebuffer m_Framebuffer;
};