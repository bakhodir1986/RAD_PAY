using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class adminViewModel
    {
        public long id { get; set; }
        public string login { get; set; }
        public string password { get; set; }
        public string first_name { get; set; }
        public int? status { get; set; }
        public string last_name { get; set; }
        public int flag { get; set; }
        public string phone { get; set; }
    }
}