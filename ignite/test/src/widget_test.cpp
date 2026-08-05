// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite/graphics/ui/widget_canvas.hpp"
#include "ignite/graphics/ui/widget_container.hpp"
#include "ignite/graphics/ui/widget_button.hpp"
#include "ignite/graphics/ui/widget_label.hpp"
#include "ignite/graphics/ui/widget_image.hpp"
#include "ignite/core/vfs/vfs.hpp"

#include <gtest/gtest.h>
#include <filesystem>

using namespace ignite;

class WidgetSystemTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        std::filesystem::create_directories("test-resources/temp");
    }
};

// -----------------------------------------------------------------------------
// 1. Tree Operations: Creation, Nested Addition, and Recursive Item Removal
// -----------------------------------------------------------------------------
TEST_F(WidgetSystemTest, TreeOperations_AddAndRecursiveRemove)
{
    Ref<WidgetCanvas> canvas = CreateRef<WidgetCanvas>();
    WidgetContainer *root = canvas->CreateRoot(1920, 1080);
    ASSERT_NE(root, nullptr);

    // Add Container A to Root
    WidgetID containerA_ID = canvas->AddContainer(root);
    WidgetContainer *containerA = canvas->GetItems().at(containerA_ID)->As<WidgetContainer>().get();
    ASSERT_NE(containerA, nullptr);

    // Add Children into Container A
    WidgetID btnID = canvas->AddButton(containerA, "Child Button");
    WidgetID imgID = canvas->AddImage(containerA);

    // Add Container B into Container A (nested container)
    WidgetID containerB_ID = canvas->AddContainer(containerA);
    WidgetContainer *containerB = canvas->GetItems().at(containerB_ID)->As<WidgetContainer>().get();
    ASSERT_NE(containerB, nullptr);

    // Add Child into Container B
    WidgetID labelID = canvas->AddLabel(containerB, "Deep Label");

    // Verify initial tree state
    EXPECT_EQ(canvas->GetItems().size(), 6u); // Root + ContainerA + Btn + Img + ContainerB + Label
    EXPECT_EQ(containerA->children.size(), 3u);
    EXPECT_EQ(containerB->children.size(), 1u);

    // Remove Container A from canvas
    bool removed = canvas->RemoveItem(containerA_ID);
    EXPECT_TRUE(removed);

    // Verify Container A AND ALL descendant children (Btn, Img, ContainerB, Label) were cleaned up
    EXPECT_EQ(canvas->GetItems().size(), 1u); // Only Root remains
    EXPECT_TRUE(root->children.empty());
    EXPECT_EQ(canvas->GetItems().find(containerA_ID), canvas->GetItems().end());
    EXPECT_EQ(canvas->GetItems().find(btnID), canvas->GetItems().end());
    EXPECT_EQ(canvas->GetItems().find(imgID), canvas->GetItems().end());
    EXPECT_EQ(canvas->GetItems().find(containerB_ID), canvas->GetItems().end());
    EXPECT_EQ(canvas->GetItems().find(labelID), canvas->GetItems().end());
}

// -----------------------------------------------------------------------------
// 2. Tree Reordering: MoveUp, MoveDown, ReorderItem
// -----------------------------------------------------------------------------
TEST_F(WidgetSystemTest, TreeReordering_MoveAndReorder)
{
    Ref<WidgetCanvas> canvas = CreateRef<WidgetCanvas>();
    WidgetContainer *root = canvas->CreateRoot(1920, 1080);

    WidgetID id1 = canvas->AddButton(root, "Btn 1");
    WidgetID id2 = canvas->AddButton(root, "Btn 2");
    WidgetID id3 = canvas->AddButton(root, "Btn 3");

    ASSERT_EQ(root->children.size(), 3u);
    EXPECT_EQ(root->children[0]->id, id1);
    EXPECT_EQ(root->children[1]->id, id2);
    EXPECT_EQ(root->children[2]->id, id3);

    // Test MoveItemUp on Btn 2 (should swap with Btn 1)
    EXPECT_TRUE(canvas->MoveItemUp(id2));
    EXPECT_EQ(root->children[0]->id, id2);
    EXPECT_EQ(root->children[1]->id, id1);
    EXPECT_EQ(root->children[2]->id, id3);

    // Test MoveItemDown on Btn 2 (should swap with Btn 1)
    EXPECT_TRUE(canvas->MoveItemDown(id2));
    EXPECT_EQ(root->children[0]->id, id1);
    EXPECT_EQ(root->children[1]->id, id2);
    EXPECT_EQ(root->children[2]->id, id3);

    // Test ReorderItem: Move Btn 3 before Btn 1
    EXPECT_TRUE(canvas->ReorderItem(id3, id1, false));
    EXPECT_EQ(root->children[0]->id, id3);
    EXPECT_EQ(root->children[1]->id, id1);
    EXPECT_EQ(root->children[2]->id, id2);
}

// -----------------------------------------------------------------------------
// 3. Flex Layout & Alignment: VerticalAlignment, HorizontalAlignment, PositionType
// -----------------------------------------------------------------------------
TEST_F(WidgetSystemTest, LayoutAndAlignment_VerticalAndHorizontalAnchoring)
{
    Ref<WidgetCanvas> canvas = CreateRef<WidgetCanvas>();
    WidgetContainer *root = canvas->CreateRoot(1000, 500);
    root->layout.flex.direction = FlexDirection::Row;
    root->layout.padding = glm::vec4(0.0f);

    // 1. Row Container AlignItems::Center test
    WidgetID rowContainerID = canvas->AddContainer(root);
    Ref<WidgetContainer> rowContainer = canvas->GetItems().at(rowContainerID)->As<WidgetContainer>();
    rowContainer->layout.flex.direction = FlexDirection::Row;
    rowContainer->layout.flex.alignItems = AlignItems::Center;
    rowContainer->layout.width = 1000.0f;
    rowContainer->layout.height = 400.0f;

    WidgetID rowChildID = canvas->AddButton(rowContainer.get(), "Centered Row Child");
    Ref<WidgetButton> rowChild = canvas->GetItems().at(rowChildID)->As<WidgetButton>();
    rowChild->layout.width = 100.0f;
    rowChild->layout.height = 50.0f;

    // 2. Absolute positioned item anchored to Bottom-Right of root container
    WidgetID btnAbsID = canvas->AddButton(root, "Absolute Bottom Right");
    Ref<WidgetButton> btnAbs = canvas->GetItems().at(btnAbsID)->As<WidgetButton>();
    btnAbs->layout.positionType = PositionType::Absolute;
    btnAbs->layout.width = 200.0f;
    btnAbs->layout.height = 80.0f;
    btnAbs->layout.horizontalAlignment = HorizontalAlignment::Right;
    btnAbs->layout.verticalAlignment = VerticalAlignment::Bottom;

    // Arrange layout
    Rect parentRect = { glm::vec2(0.0f), glm::vec2(1000.0f, 500.0f) };
    root->Arrange(parentRect);

    // Verify Row container AlignItems::Center centers child vertically inside 400px container ((400 - 50)/2 = 175)
    EXPECT_FLOAT_EQ(rowChild->worldRect.min.y, 175.0f);
    EXPECT_FLOAT_EQ(rowChild->worldRect.max.y, 225.0f);

    // Verify Absolute Bottom-Right item rect: min.x = (1000 - 200 = 800), min.y = (500 - 80 = 420)
    EXPECT_FLOAT_EQ(btnAbs->worldRect.min.x, 800.0f);
    EXPECT_FLOAT_EQ(btnAbs->worldRect.min.y, 420.0f);
    EXPECT_FLOAT_EQ(btnAbs->worldRect.max.x, 1000.0f);
    EXPECT_FLOAT_EQ(btnAbs->worldRect.max.y, 500.0f);
}

// -----------------------------------------------------------------------------
// 4. Canvas Root Defaults: SpaceBetween Layout
// -----------------------------------------------------------------------------
TEST_F(WidgetSystemTest, CanvasRoot_DefaultSpaceBetween)
{
    Ref<WidgetCanvas> canvas = CreateRef<WidgetCanvas>();
    WidgetContainer *root = canvas->CreateRoot(1920, 1080);
    EXPECT_EQ(root->layout.flex.direction, FlexDirection::Column);
    EXPECT_EQ(root->layout.flex.justifyContent, JustifyContent::SpaceBetween);

    // Add Top BoxSizing and Bottom BoxSizing
    WidgetID topID = canvas->AddBoxSizing(root);
    Ref<IWidgetItem> topBox = canvas->GetItems().at(topID);
    topBox->layout.width = 1920.0f;
    topBox->layout.height = 60.0f;

    WidgetID bottomID = canvas->AddBoxSizing(root);
    Ref<IWidgetItem> bottomBox = canvas->GetItems().at(bottomID);
    bottomBox->layout.width = 1920.0f;
    bottomBox->layout.height = 100.0f;

    Rect canvasArea = { glm::vec2(0.0f), glm::vec2(1920.0f, 1080.0f) };
    root->Arrange(canvasArea);

    // Verify Top BoxSizing is at top (y = 0)
    EXPECT_FLOAT_EQ(topBox->worldRect.min.y, 0.0f);
    EXPECT_FLOAT_EQ(topBox->worldRect.max.y, 60.0f);

    // Verify Bottom BoxSizing is pushed to the bottom (1080 - 100 = 980)
    EXPECT_FLOAT_EQ(bottomBox->worldRect.min.y, 980.0f);
    EXPECT_FLOAT_EQ(bottomBox->worldRect.max.y, 1080.0f);
}

// -----------------------------------------------------------------------------
// 4. Serialization Roundtrip
// -----------------------------------------------------------------------------
TEST_F(WidgetSystemTest, Serialization_FullRoundtrip)
{
    ignite::Path filepath = "test-resources/temp/widget_full_test.wdgt";

    Ref<WidgetCanvas> canvasSrc = CreateRef<WidgetCanvas>();
    WidgetContainer *root = canvasSrc->CreateRoot(1920, 1080);

    WidgetID c1ID = canvasSrc->AddContainer(root);
    Ref<WidgetContainer> c1 = canvasSrc->GetItems().at(c1ID)->As<WidgetContainer>();
    c1->name = "Main Box";
    c1->layout.flex.direction = FlexDirection::Column;
    c1->layout.verticalAlignment = VerticalAlignment::Middle;
    c1->layout.horizontalAlignment = HorizontalAlignment::Center;

    WidgetID btnID = canvasSrc->AddButton(c1.get(), "Play Game");
    Ref<WidgetButton> btn = canvasSrc->GetItems().at(btnID)->As<WidgetButton>();
    btn->style.color = glm::vec4(0.2f, 0.8f, 0.2f, 1.0f);

    WidgetID labelID = canvasSrc->AddLabel(c1.get(), "Title Label");
    Ref<WidgetLabel> lbl = canvasSrc->GetItems().at(labelID)->As<WidgetLabel>();
    lbl->style.fontSize = 24.0f;

    WidgetID imgID = canvasSrc->AddImage(c1.get());
    WidgetID boxID = canvasSrc->AddBoxSizing(c1.get());
    WidgetID ovrID = canvasSrc->AddOverlay(c1.get());

    // Serialize
    ASSERT_TRUE(canvasSrc->Serialize(filepath));

    // Deserialize into new canvas
    Ref<WidgetCanvas> canvasDst = WidgetCanvas::Deserialize(filepath);
    ASSERT_NE(canvasDst, nullptr);

    // Verify deserialized canvas content match
    EXPECT_EQ(canvasDst->GetItems().size(), canvasSrc->GetItems().size());
    ASSERT_NE(canvasDst->GetRoot(), nullptr);

    WidgetContainer *dstRoot = canvasDst->GetRoot();
    ASSERT_EQ(dstRoot->children.size(), 1u);

    Ref<WidgetContainer> dstC1 = dstRoot->children[0]->As<WidgetContainer>();
    ASSERT_NE(dstC1, nullptr);
    EXPECT_EQ(dstC1->name, "Main Box");
    EXPECT_EQ(dstC1->layout.flex.direction, FlexDirection::Column);
    EXPECT_EQ(dstC1->children.size(), 5u); // Btn, Label, Image, BoxSizing, Overlay

    Ref<WidgetButton> dstBtn = dstC1->children[0]->As<WidgetButton>();
    ASSERT_NE(dstBtn, nullptr);
    EXPECT_EQ(dstBtn->label ? dstBtn->label->text : "", "Play Game");
    EXPECT_FLOAT_EQ(dstBtn->style.color.g, 0.8f);

    Ref<WidgetLabel> dstLbl = dstC1->children[1]->As<WidgetLabel>();
    ASSERT_NE(dstLbl, nullptr);
    EXPECT_EQ(dstLbl->text, "Title Label");
    EXPECT_FLOAT_EQ(dstLbl->style.fontSize, 24.0f);
}
