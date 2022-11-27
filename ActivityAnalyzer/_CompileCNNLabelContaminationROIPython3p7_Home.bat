cd "C:\Users\antho\Anaconda3\Scripts"
pyinstaller --onefile "C:\Users\antho\Documents\Autogans\ActivityAnalyzer\ActivityAnalyzer\CNN_LabelContaminationManually.py"
copy "C:\Users\antho\Anaconda3\Scripts\dist\CNN_LabelContaminationManually.exe" "C:\Users\antho\Documents\Autogans\ActivityAnalyzer\ActivityAnalyzer\dist\CNN_LabelContaminationManually.exe"
cmd /k