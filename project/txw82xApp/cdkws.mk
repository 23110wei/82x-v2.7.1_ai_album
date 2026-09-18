.PHONY: clean All Project_Title Project_PreBuild Project_Build Project_PostBuild

All: Project_Title Project_PreBuild Project_Build Project_PostBuild

Project_Title:
	@echo "----------Building project:[ txw82xApp - FLASH ]----------"

Project_PreBuild:
	@echo Executing Pre Build commands ...
	@export CDKPath="D:/C-Sky/CDK" CDK_VERSION="V2.24.14" CPU="E804DF" ProjectName="txw82xApp" ProjectPath="D:/work/Dual_Screen_Cmake/TXW82x_FPV-v2.7.1.7-45228/TXW82x_FPV-v2.7.1.7-45228/project/txw82xApp/" && D:/work/Dual_Screen_Cmake/TXW82x_FPV-v2.7.1.7-45228/TXW82x_FPV-v2.7.1.7-45228/project/txw82xApp/prebuild.sh $<
	@echo Done

Project_Build:
	@make -r -f txw82xApp.mk -C  ./ 

Project_PostBuild:
	@echo Executing Post Build commands ...
	@export CDKPath="D:/C-Sky/CDK" CDK_VERSION="V2.24.14" CPU="E804DF" ProjectName="txw82xApp" ProjectPath="D:/work/Dual_Screen_Cmake/TXW82x_FPV-v2.7.1.7-45228/TXW82x_FPV-v2.7.1.7-45228/project/txw82xApp/" && D:/work/Dual_Screen_Cmake/TXW82x_FPV-v2.7.1.7-45228/TXW82x_FPV-v2.7.1.7-45228/project/txw82xApp/BuildBIN.sh
	@echo Done


clean:
	@echo "----------Cleaning project:[ txw82xApp - FLASH ]----------"

