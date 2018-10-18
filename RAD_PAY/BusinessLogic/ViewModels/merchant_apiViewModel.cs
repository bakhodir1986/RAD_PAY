using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class merchant_apiViewModel
    {
        public int id { get; set; }
        public string name { get; set; }
        public string url { get; set; }
        public string login { get; set; }
        public string password { get; set; }
        public string api_json { get; set; }
        public string options { get; set; }
        public int? status { get; set; }
        public int api_id { get; set; }
        public int? b { get; set; }
    }
}