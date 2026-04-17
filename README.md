# Air Defense System

## কন্ট্রিবিউশন গাইড

এই প্রজেক্টে কন্ট্রিবিউট করতে নিচের ধাপগুলো অনুসরণ করো:

---

### Step 1: রিপোজিটরি ক্লোন করো
```bash
git clone https://github.com/Ashraf1551/air-defense-system.git
```

---

### Step 2: নতুন ব্রাঞ্চ তৈরি করো (তোমার nickname ব্যবহার করো)
```bash
git checkout -b your_nickname
```

উদাহরণ:
```bash
git checkout -b ashraf
```

---

### Step 3: ব্রাঞ্চ পুশ করো
```bash
git push -u origin your_nickname
```

উদাহরণ:
```bash
git push -u origin ashraf
```

---

### Step 4: Main থেকে latest update নেওয়া (Merge)
কাজ শুরু করার আগে বা নিয়মিতভাবে main branch এর নতুন পরিবর্তন নিজের ব্রাঞ্চে আনতে হবে:

```bash
git checkout your_nickname
git merge main
```

উদাহরণ:
```bash
git checkout ashraf
git merge main
```

---

### Step 4: পরিবর্তন commit এবং push করা

কোনো কাজ করার পর নিচের কমান্ডগুলো ব্যবহার করো:

```bash
git add .
git commit -m "give a meaningful commit message"
git push -u origin your_branch_name
```

## নিয়মাবলী
- কাজ করার আগে অবশ্যই নিজের ব্রাঞ্চ তৈরি করতে হবে  
- সরাসরি `main` ব্রাঞ্চে push করা যাবে না  
- প্রতিটি পরিবর্তনের আগে `main` থেকে latest update নিয়ে নিতে হবে  
- commit message পরিষ্কার এবং meaningful হতে হবে  